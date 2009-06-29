// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SQLITEUTILS_H_
#define CHROME_COMMON_SQLITEUTILS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/string_util.h"

#include "third_party/sqlite/preprocessed/sqlite3.h"

// forward declarations of classes defined here
class FilePath;
class SQLTransaction;
class SQLNestedTransaction;
class SQLNestedTransactionSite;
class scoped_sqlite3_stmt_ptr;
class SQLStatement;

//------------------------------------------------------------------------------
// A wrapper for sqlite transactions that rollsback when the wrapper
// goes out of scope if the caller has not already called Commit or Rollback.
// Note: the constructor does NOT Begin a transaction.
//------------------------------------------------------------------------------
class SQLTransaction {
 public:
  explicit SQLTransaction(sqlite3* db);
  virtual ~SQLTransaction();

  int Begin() {
    // By default, we BEGIN IMMEDIATE to establish file locks at the
    // onset of a transaction. This avoids SQLITE_BUSY errors, without
    // waiting for the busy timeout period, which can occur when BEGIN
    // DEFERRED is used.
    return BeginImmediate();
  }

  int BeginExclusive() {
    return BeginCommand("BEGIN EXCLUSIVE");
  }

  int BeginImmediate() {
    return BeginCommand("BEGIN IMMEDIATE");
  }

  int BeginDeferred() {
    return BeginCommand("BEGIN DEFERRED");
  }

  int Commit() {
    return EndCommand("COMMIT");
  }

  int Rollback() {
    return EndCommand("ROLLBACK");
  }

  bool HasBegun() {
    return began_;
  }

 protected:
  virtual int BeginCommand(const char* command);
  virtual int EndCommand(const char* command);

  sqlite3* db_;
  bool began_;
  DISALLOW_COPY_AND_ASSIGN(SQLTransaction);
};


//------------------------------------------------------------------------------
// A class for use with SQLNestedTransaction.
//------------------------------------------------------------------------------
class SQLNestedTransactionSite {
 protected:
  SQLNestedTransactionSite() : db_(NULL), top_transaction_(NULL) {}
  virtual ~SQLNestedTransactionSite();

  // The following virtual methods provide notification of true transaction
  // boundaries as they are crossed by a top nested transaction.
  // Intended to be overriden (See WebCacheDB)
  // SQLNestedTransaction calls these after the underlying database
  // operation has been performed.

  virtual void OnBegin() {}
  virtual void OnCommit() {}
  virtual void OnRollback() {}

  // Returns the sqlite3 database connection associated with this site
  // Used by SQLNestedTransaction
  sqlite3* GetSqlite3DB() { return db_; }

  // Returns the current top nested transaction associated with this site
  // Used by SQLNestedTransaction
  SQLNestedTransaction* GetTopTransaction() {
    return top_transaction_;
  }

  // Sets or clears the top nested transaction associated with this site
  // Used by SQLNestedTransaction
  void SetTopTransaction(SQLNestedTransaction* top);

  sqlite3* db_;
  SQLNestedTransaction* top_transaction_;
  friend class SQLNestedTransaction;
};

//------------------------------------------------------------------------------
// SQLite does not support nested transactions. This class provides a gross
// approximation of nested transactions.
//
// Really there is only one transaction, the top transaction.
//
// A nested transaction commits with respect to the top transaction.
// That is, even though the nested transaction commits, the permanence of its
// effects depends on the top transaction committing. If the top
// transaction rollsback, the results of the nested transaction are backed out.
// If any nested transaction aborts, the top transaction ultimately rollsback
// as well.
//
// Note: If a nested transaction is open for a particular db connection, an
// attempt to open a non-nested transaction (class SQLTransaction) will fail.
// And vice versa.
//
// TODO(michaeln): demonstrate usage here
// TODO(michaeln): safegaurds to prevent mis-use
//------------------------------------------------------------------------------
class SQLNestedTransaction : public SQLTransaction {
 public:
  explicit SQLNestedTransaction(SQLNestedTransactionSite* site);
  virtual ~SQLNestedTransaction();

 protected:
  virtual int BeginCommand(const char* command);
  virtual int EndCommand(const char* command);

 private:
  bool needs_rollback_;
  SQLNestedTransactionSite* site_;
  DISALLOW_COPY_AND_ASSIGN(SQLNestedTransaction);
};

//------------------------------------------------------------------------------
// A scoped sqlite statement that finalizes when it goes out of scope.
//------------------------------------------------------------------------------
class scoped_sqlite3_stmt_ptr {
 public:
  ~scoped_sqlite3_stmt_ptr() {
    finalize();
  }

  scoped_sqlite3_stmt_ptr() : stmt_(NULL) {
  }

  explicit scoped_sqlite3_stmt_ptr(sqlite3_stmt* stmt)
    : stmt_(stmt) {
  }

  sqlite3_stmt* get() const {
    return stmt_;
  }

  void set(sqlite3_stmt* stmt) {
    finalize();
    stmt_ = stmt;
  }

  sqlite3_stmt* release() {
    sqlite3_stmt* tmp = stmt_;
    stmt_ = NULL;
    return tmp;
  }

  // It is not safe to call sqlite3_finalize twice on the same stmt.
  // Sqlite3's sqlite3_finalize() function should not be called directly
  // without calling the release method.  If sqlite3_finalize() must be
  // called directly, the following usage is advised:
  //  scoped_sqlite3_stmt_ptr stmt;
  //  ... do something with stmt ...
  //  sqlite3_finalize(stmt.release());
  int finalize() {
    int err = sqlite3_finalize(stmt_);
    stmt_ = NULL;
    return err;
  }

 protected:
  sqlite3_stmt* stmt_;

 private:
  DISALLOW_COPY_AND_ASSIGN(scoped_sqlite3_stmt_ptr);
};


//------------------------------------------------------------------------------
// A scoped sqlite statement with convenient C++ wrappers for sqlite3 APIs.
//------------------------------------------------------------------------------
class SQLStatement : public scoped_sqlite3_stmt_ptr {
 public:
  SQLStatement() {
  }

  int prepare(sqlite3* db, const char* sql) {
    return prepare(db, sql, -1);
  }

  int prepare(sqlite3* db, const char* sql, int sql_len);

  int step();
  int reset();
  sqlite_int64 last_insert_rowid();
  sqlite3* db_handle();

  //
  // Parameter binding helpers (NOTE: index is 0-based)
  //

  int bind_parameter_count();

  typedef void (*Function)(void*);

  int bind_blob(int index, std::vector<unsigned char>* blob);
  int bind_blob(int index, const void* value, int value_len);
  int bind_blob(int index, const void* value, int value_len, Function dtor);
  int bind_double(int index, double value);
  int bind_bool(int index, bool value);
  int bind_int(int index, int value);
  int bind_int64(int index, sqlite_int64 value);
  int bind_null(int index);

  int bind_string(int index, const std::string& value) {
    // don't use c_str so it doesn't have to fix up the null terminator
    // (sqlite just uses the length)
    return bind_text(index, value.data(),
                     static_cast<int>(value.length()), SQLITE_TRANSIENT);
  }

  int bind_wstring(int index, const std::wstring& value) {
    // don't use c_str so it doesn't have to fix up the null terminator
    // (sqlite just uses the length)
    std::string value_utf8(WideToUTF8(value));
    return bind_text(index, value_utf8.data(),
                     static_cast<int>(value_utf8.length()), SQLITE_TRANSIENT);
  }

  int bind_text(int index, const char* value) {
    return bind_text(index, value, -1, SQLITE_TRANSIENT);
  }

  // value_len is number of characters or may be negative
  // a for null-terminated value string
  int bind_text(int index, const char* value, int value_len) {
    return bind_text(index, value, value_len, SQLITE_TRANSIENT);
  }

  // value_len is number of characters or may be negative
  // a for null-terminated value string
  int bind_text(int index, const char* value, int value_len,
                Function dtor);

  int bind_text16(int index, const char16* value) {
    return bind_text16(index, value, -1, SQLITE_TRANSIENT);
  }

  // value_len is number of characters or may be negative
  // a for null-terminated value string
  int bind_text16(int index, const char16* value, int value_len) {
    return bind_text16(index, value, value_len, SQLITE_TRANSIENT);
  }

  // value_len is number of characters or may be negative
  // a for null-terminated value string
  int bind_text16(int index, const char16* value, int value_len,
                  Function dtor);

  int bind_value(int index, const sqlite3_value* value);

  //
  // Column helpers (NOTE: index is 0-based)
  //

  int column_count();
  int column_type(int index);
  const void* column_blob(int index);
  bool column_blob_as_vector(int index, std::vector<unsigned char>* blob);
  bool column_blob_as_string(int index, std::string* blob);
  int column_bytes(int index);
  int column_bytes16(int index);
  double column_double(int index);
  bool column_bool(int index);
  int column_int(int index);
  sqlite_int64 column_int64(int index);
  const char* column_text(int index);
  bool column_string(int index, std::string* str);
  std::string column_string(int index);
  const char16* column_text16(int index);
  bool column_wstring(int index, std::wstring* str);
  std::wstring column_wstring(int index);

 private:
  DISALLOW_COPY_AND_ASSIGN(SQLStatement);
};

// TODO(estade): wrap the following static functions in a namespace.

// Opens the DB in the file pointed to by |filepath|.  This method forces the
// database to be in UTF-8 mode on all platforms. See
// http://www.sqlite.org/capi3ref.html#sqlite3_open for an explanation of the
// return value.
int OpenSqliteDb(const FilePath& filepath, sqlite3** database);

// Returns true if there is a table with the given name in the database.
// For the version where a database name is specified, it may be NULL or the
// empty string if no database name is necessary.
bool DoesSqliteTableExist(sqlite3* db,
                          const char* db_name,
                          const char* table_name);
inline bool DoesSqliteTableExist(sqlite3* db, const char* table_name) {
  return DoesSqliteTableExist(db, NULL, table_name);
}

// Test whether a table has a column matching the provided name and type.
// Returns true if the column exist and false otherwise. There are two
// versions, one that takes a database name, the other that doesn't. The
// database name can be NULL or empty if no name is desired.
//
// Column type is optional, it can be NULL or empty. If specified, we the
// function will check that the column is of the correct type (case-sensetive).
bool DoesSqliteColumnExist(sqlite3* db,
                           const char* datbase_name,
                           const char* table_name,
                           const char* column_name,
                           const char* column_type);
inline bool DoesSqliteColumnExist(sqlite3* db,
                           const char* table_name,
                           const char* column_name,
                           const char* column_type) {
  return DoesSqliteColumnExist(db, NULL, table_name, column_name, column_type);
}

// Test whether a table has one or more rows. Returns true if the table
// has one or more rows and false if the table is empty or doesn't exist.
bool DoesSqliteTableHaveRow(sqlite3* db, const char* table_name);

#endif  // CHROME_COMMON_SQLITEUTILS_H_
