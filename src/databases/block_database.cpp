/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/database/databases/block_database.hpp>

#include <cstdint>
#include <cstddef>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/database/memory/memory.hpp>
#include <bitcoin/database/result/block_result.hpp>

namespace libbitcoin {
namespace database {

using namespace bc::chain;

// Valid file offsets should never be zero.
const file_offset block_database::empty = 0;

static constexpr auto index_header_size = 0u;
static constexpr auto index_record_size = sizeof(file_offset);

// Record format:
// main:
//  [ header:80      ]
//  [ height:4       ]
//  [ number_txs:1-8 ]
// hashes:
//  [ [    ...     ] ]
//  [ [ tx_hash:32 ] ]
//  [ [    ...     ] ]

static constexpr char insert_block_sql[] = "INSERT INTO blocks (hash, height, version, prev_block, merkle, timestamp, bits, nonce) VALUES (?1, ?2, ?3, ?4, ?5 , ?6, ?7, ?8);";

static constexpr char select_block_by_hash_sql[] = "SELECT id, hash, height, version, prev_block, merkle, timestamp, bits, nonce FROM blocks WHERE hash = ?1;";
static constexpr char select_block_by_height_sql[] = "SELECT id, hash, height, version, prev_block, merkle, timestamp, bits, nonce FROM blocks WHERE id = ?1;";

static constexpr char get_max_height_block_sql[] = "SELECT max(height) FROM blocks;";

static constexpr char exists_block_by_height_sql[] = "SELECT 1 FROM blocks WHERE id= ?1;";

static constexpr char delete_block_sql[] = "DELETE FROM blocks WHERE hash = ?1;";
static constexpr char delete_block_sql[] = "DELETE FROM blocks WHERE height = ?1;";

/*
    block_db.exec("CREATE TABLE blocks( "
            "id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, "
            "hash TEXT NOT NULL UNIQUE, "
            "version INTEGER NOT NULL, "
            "prev_block TEXT NOT NULL , "
            "merkle TEXT NOT NULL , "
            "timestamp INTEGER NOT NULL, "
            "bits INTEGER NOT NULL, "
            "nonce INTEGER NOT NULL);"
*/

// Blocks uses a hash table and an array index, both O(1).
block_database::block_database(const path& filename)
  : block_db(filename.c_str())
{
    prepare_statements();
}

block_database::~block_database()
{
    close();
}

// Create.
// ----------------------------------------------------------------------------

// Initialize files and open.
bool block_database::create()
{
    block_db.exec("CREATE TABLE blocks( "
            "id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, "
            "hash TEXT NOT NULL UNIQUE, "
            "height INTEGER NOT NULL UNIQUE, "
            "version INTEGER NOT NULL, "
            "prev_block TEXT NOT NULL , "
            "merkle TEXT NOT NULL , "
            "timestamp INTEGER NOT NULL, "
            "bits INTEGER NOT NULL, "
            "nonce INTEGER NOT NULL);", [&res](int reslocal, std::string const& error_msg){

        if (reslocal != SQLITE_OK) {
            //TODO: Fer: Log errors
            res = false;
        }
    });

    res = prepare_statements();

    return res;
 
}

bool block_database::prepare_statements() {
    std::cout << "bool transaction_database::prepare_statements()\n";
    int rc;

    //INSERT BLOCK STATEMENT
    rc = sqlite3_prepare_v2(block_db.ptr(), insert_block_sql, -1, &insert_block_stmt_, NULL);
    std::cout << "rc: " << rc << '\n';
    printf("ERROR: %s\n", sqlite3_errmsg(block_db.ptr()));

    //SELECT BLOCK BY HASH STATEMENT
    rc = sqlite3_prepare_v2(block_db.ptr(), select_block_by_hash_sql, -1, &select_block_by_hash_stmt_, NULL);
    std::cout << "rc: " << rc << '\n';
    printf("ERROR: %s\n", sqlite3_errmsg(block_db.ptr()));
    
    //SELECT BLOCK BY HEIGHT STATEMENT
    rc = sqlite3_prepare_v2(block_db.ptr(), select_block_by_height_sql, -1, &select_block_by_height_stmt_, NULL);
    std::cout << "rc: " << rc << '\n';
    printf("ERROR: %s\n", sqlite3_errmsg(block_db.ptr()));

    //EXISTS BLOCK BY HEIGHT STATEMENT
    rc = sqlite3_prepare_v2(block_db.ptr(), exists_block_by_height_sql, -1, &exists_block_by_height_stmt_, NULL);
    std::cout << "rc: " << rc << '\n';
    printf("ERROR: %s\n", sqlite3_errmsg(block_db.ptr()));

    //GET MAX HEIGHT BLOCK
    rc = sqlite3_prepare_v2(block_db.ptr(), get_max_height_block_sql, -1, &get_max_height_block_stmt_, NULL);
    std::cout << "rc: " << rc << '\n';
    printf("ERROR: %s\n", sqlite3_errmsg(block_db.ptr()));
    
    //DELETE BLOCK STATEMENT
    rc = sqlite3_prepare_v2(block_db.ptr(), delete_block_sql, -1, &delete_block_stmt_, NULL);
    std::cout << "rc: " << rc << '\n';
    printf("ERROR: %s\n", sqlite3_errmsg(block_db.ptr()));

    //TODO: Fer: check for errors

    std::cout << "bool block_database::prepare_statements() -- END\n";

    return true;

}

// Startup and shutdown.
// ----------------------------------------------------------------------------

// Start files and primitives.
bool block_database::open()
{
  std::cout << "bool block_database::open()\n";
  return prepare_statements();
}

// Close files.
bool block_database::close()
{
    std::cout << "bool block_database::close()\n";
    return true;
}

// Commit latest inserts.
void block_database::synchronize()
{

}

// Flush the memory maps to disk.
bool block_database::flush()
{
  return true;
}

// Queries.
// ----------------------------------------------------------------------------

bool block_database::exists(size_t height) const
{
  sqlite3_reset(exists_block_by_height_stmt_);
  sqlite3_bind_int(exists_block_by_height_stmt_, 1, height, SQLITE_STATIC);
  int rc = sqlite3_step(exists_block_by_height_stmt_);
  return rc == SQLITE_ROW;
}

block_result block_database::get(size_t height) const
{
  sqlite3_reset(select_block_by_height_stmt_);
  sqlite3_bind_int(select_block_by_height_stmt_, 1, height, SQLITE_STATIC);
  return get(select_block_by_height_stmt_);
}

block_result block_database::get(const hash_digest& hash) const
{
  sqlite3_reset(select_block_by_hash_stmt_);
  sqlite3_bind_text(select_block_by_hash_stmt_, 1, hash, SQLITE_STATIC);
  return get(select_block_by_hash_stmt_);
}

block_result block_database::get(sqlite3_stmt* stmt) const
{  
  int rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) 
  {

    auto id = sqlite3_column_int32(stmt, 0);
    
    hash_digest hash;
    memcpy(hash.data(), sqlite3_column_text(stmt, 1), sizeof(hash));
    
    auto height = static_cast<uint32_t>(sqlite3_column_int(stmt, 2));
    
    auto version = static_cast<uint32_t>(sqlite3_column_int(stmt, 3));

    hash_digest prev_block;
    memcpy(prev_block.data(), sqlite3_column_text(stmt, 4), sizeof(prev_block));

    hash_digest merkle;
    memcpy(merkle.data(), sqlite3_column_text(stmt, 5), sizeof(merkle));

    auto timestamp = static_cast<uint32_t>sqlite3_column_int32(stmt, 6);
    auto bits = static_cast<uint32_t>sqlite3_column_int32(stmt, 7);
    auto nonce = static_cast<uint32_t>sqlite3_column_int32(stmt, 8);

    std::vector<hash_digest> tx_hashes;

    //TODO GET TX_HASHES FROM TRANSACTION DATABASE
    /*
      static constexpr char select_transactions_from_block_sql[] = "SELECT hash FROM transactions WHERE block_height = ?1 ORDER BY position;";
      
      rc = sqlite3_prepare_v2(tx_db.ptr(), select_transactions_from_block_sql, -1, &select_transactions_from_block_stmt_, NULL);
      std::cout << "rc: " << rc << '\n';
      printf("ERROR: %s\n", sqlite3_errmsg(tx_db.ptr()));
      
      vector<hash_digest> transaction_database::get_transactions_from_block(size_t height)
      {
        sqlite3_reset(select_transactions_from_block_stmt_);
        sqlite3_bind_int32(select_transactions_from_block_stmt_, 1, height, SQLITE_STATIC);
        int rc = sqlite3_step(select_transactions_from_block_stmt_);
        vector<hash_digest> transaction_hashes;
        if(rc == SQLITE_ROW)
        {
          while(rc == SQLITE_ROW)
          {
            hash_digest tx_hash;
            memcpy(tx_hash.data(), sqlite3_column_text(stmt, 0), sizeof(tx_hash));
            transaction_hashes.push_back(tx_hash);
          }
        else 
          {
          printf("ERROR: %s\n", sqlite3_errmsg(tx_db.ptr()));
          }
        }
        
        return transaction_hashes;
      }
    
    */

    // tx_hashes = transaction_database::get_transactions_from_block(height);

    chain::header block_header(version,prev_block, merkle, timestamp, bits, nonce);
    return block_result(true, height,hash, block_header,tx_hashes);


  } else if (rc == SQLITE_DONE) 
    {

      std::cout << "block_result block_database::get(sqlite3_stmt* stmt) const -- END no data found\n";
      hash_digest hash;
      return block_result(false, uint32_t(), hash, chain::header());

    } else 
    {
      std::cout << "block_result block_database::get(sqlite3_stmt* stmt) const -- END with error\n";
      hash_digest hash;
      return block_result(false, uint32_t(), hash, chain::header());
    }
 
}

void block_database::store(const block& block, size_t height)
{
  BITCOIN_ASSERT(height <= max_uint32);
  const auto height32 = static_cast<uint32_t>(height);
  
  sqlite3_reset(insert_block_stmt_);
  //static constexpr char insert_block_sql[] = "INSERT INTO blocks (hash, version, prev_block, merkle, timestamp, bits, nonce) VALUES (?1, ?2, ?3, ?4, ?5 , ?6, ?7);";
  sqlite3_bind_text(insert_block_stmt_, 1, reinterpret_cast<char const*>(block.header().hash()), sizeof(hash_digest), SQLITE_STATIC);
  sqlite3_bind_int(insert_block_stmt_, 2, height);
  sqlite3_bind_int(insert_block_stmt_, 3, block.header().version());
  sqlite3_bind_text(insert_block_stmt_, 4,  reinterpret_cast<char const*>(block.header().previous_block_hash()), sizeof(hash_digest) , SQLITE_STATIC);
  sqlite3_bind_text(insert_block_stmt_, 5,  reinterpret_cast<char const*>(block.header().merkle()), sizeof(hash_digest), SQLITE_STATIC);
  sqlite3_bind_int(insert_block_stmt_, 6, block.header().timestamp());
  sqlite3_bind_int(insert_block_stmt_, 7, block.header().bits());
  sqlite3_bind_int(insert_block_stmt_, 8, block.header().nonce());
  auto rc = sqlite3_step(insert_block_stmt_);
  if (rc != SQLITE_DONE)
  {
    std::cout<<"block_database::store(const block& block, size_t height) -- Failed to Insert \n";
  }

}

bool block_database::gaps(heights& out_gaps) const
{

   /*
    const auto count = index_manager_.count();

    for (size_t height = 0; height < count; ++height)
        if (read_position(height) == empty)
            out_gaps.push_back(height);

    return true;
    */
  //TODO RAMA 
  return true;
}

bool block_database::unlink(size_t from_height)
{
    /*
    if (index_manager_.count() > from_height)
    {
        index_manager_.set_count(from_height);
        return true;
    }

    return false;
    */

  //TODO RAMA
  return true;
}


// The index of the highest existing block, independent of gaps.
bool block_database::top(size_t& out_height) const
{
  sqlite3_reset(get_max_height_block_sql_);
  sqlite3_bind_int(get_max_height_block_sql_, 1, height, SQLITE_STATIC);
  int rc = sqlite3_step(get_max_height_block_sql_);
  if( rc == SQLITE_ROW)
  {
    out_height = static_cast<uint32_t> (sqlite3_column_int32(get_max_height_block_sql_, 0));
    return true;    
  }
  
  return false;
}

} // namespace database
} // namespace libbitcoin
