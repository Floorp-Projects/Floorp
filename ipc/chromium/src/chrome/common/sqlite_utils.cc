// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/sqlite_utils.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/string16.h"

int OpenSqliteDb(const FilePath& filepath, sqlite3** database) {
#if defined(OS_WIN)
  // We want the default encoding to always be UTF-8, so we use the
  // 8-bit version of open().
  return sqlite3_open(WideToUTF8(filepath.value()).c_str(), database);
#elif defined(OS_POSIX)
  return sqlite3_open(filepath.value().c_str(), database);
#endif
}

bool DoesSqliteTableExist(sqlite3* db,
                          const char* db_name,
                          const char* table_name) {
  // sqlite doesn't allow binding parameters as table names, so we have to
  // manually construct the sql
  std::string sql("SELECT name FROM ");
  if (db_name && db_name[0]) {
    sql.append(db_name);
    sql.push_back('.');
  }
  sql.append("sqlite_master WHERE type='table' AND name=?");

  SQLStatement statement;
  if (statement.prepare(db, sql.c_str()) != SQLITE_OK)
    return false;

  if (statement.bind_text(0, table_name) != SQLITE_OK)
    return false;

  // we only care about if this matched a row, not the actual data
  return sqlite3_step(statement.get()) == SQLITE_ROW;
}

bool DoesSqliteColumnExist(sqlite3* db,
                           const char* database_name,
                           const char* table_name,
                           const char* column_name,
                           const char* column_type) {
  SQLStatement s;
  std::string sql;
  sql.append("PRAGMA ");
  if (database_name && database_name[0]) {
    // optional database name specified
    sql.append(database_name);
    sql.push_back('.');
  }
  sql.append("TABLE_INFO(");
  sql.append(table_name);
  sql.append(")");

  if (s.prepare(db, sql.c_str()) != SQLITE_OK)
    return false;

  while (s.step() == SQLITE_ROW) {
    if (!s.column_string(1).compare(column_name)) {
      if (column_type && column_type[0])
        return !s.column_string(2).compare(column_type);
      return true;
    }
  }
  return false;
}

bool DoesSqliteTableHaveRow(sqlite3* db, const char* table_name) {
  SQLStatement s;
  std::string b;
  b.append("SELECT * FROM ");
  b.append(table_name);

  if (s.prepare(db, b.c_str()) != SQLITE_OK)
    return false;

  return s.step() == SQLITE_ROW;
}

SQLTransaction::SQLTransaction(sqlite3* db) : db_(db), began_(false) {
}

SQLTransaction::~SQLTransaction() {
  if (began_) {
    Rollback();
  }
}

int SQLTransaction::BeginCommand(const char* command) {
  int rv = SQLITE_ERROR;
  if (!began_ && db_) {
    rv = sqlite3_exec(db_, command, NULL, NULL, NULL);
    began_ = (rv == SQLITE_OK);
  }
  return rv;
}

int SQLTransaction::EndCommand(const char* command) {
  int rv = SQLITE_ERROR;
  if (began_ && db_) {
    rv = sqlite3_exec(db_, command, NULL, NULL, NULL);
    began_ = (rv != SQLITE_OK);
  }
  return rv;
}

SQLNestedTransactionSite::~SQLNestedTransactionSite() {
  DCHECK(!top_transaction_);
}

void SQLNestedTransactionSite::SetTopTransaction(SQLNestedTransaction* top) {
  DCHECK(!top || !top_transaction_);
  top_transaction_ = top;
}

SQLNestedTransaction::SQLNestedTransaction(SQLNestedTransactionSite* site)
  : SQLTransaction(site->GetSqlite3DB()),
    needs_rollback_(false),
    site_(site) {
  DCHECK(site);
  if (site->GetTopTransaction() == NULL) {
    site->SetTopTransaction(this);
  }
}

SQLNestedTransaction::~SQLNestedTransaction() {
  if (began_) {
    Rollback();
  }
  if (site_->GetTopTransaction() == this) {
    site_->SetTopTransaction(NULL);
  }
}

int SQLNestedTransaction::BeginCommand(const char* command) {
  DCHECK(db_);
  DCHECK(site_ && site_->GetTopTransaction());
  if (!db_ || began_) {
    return SQLITE_ERROR;
  }
  if (site_->GetTopTransaction() == this) {
    int rv = sqlite3_exec(db_, command, NULL, NULL, NULL);
    began_ = (rv == SQLITE_OK);
    if (began_) {
      site_->OnBegin();
    }
    return rv;
  } else {
    if (site_->GetTopTransaction()->needs_rollback_) {
      return SQLITE_ERROR;
    }
    began_ = true;
    return SQLITE_OK;
  }
}

int SQLNestedTransaction::EndCommand(const char* command) {
  DCHECK(db_);
  DCHECK(site_ && site_->GetTopTransaction());
  if (!db_ || !began_) {
    return SQLITE_ERROR;
  }
  if (site_->GetTopTransaction() == this) {
    if (needs_rollback_) {
      sqlite3_exec(db_, "ROLLBACK", NULL, NULL, NULL);
      began_ = false;  // reset so we don't try to rollback or call
                       // OnRollback() again
      site_->OnRollback();
      return SQLITE_ERROR;
    } else {
      int rv = sqlite3_exec(db_, command, NULL, NULL, NULL);
      began_ = (rv != SQLITE_OK);
      if (strcmp(command, "ROLLBACK") == 0) {
        began_ = false;  // reset so we don't try to rollbck or call
                         // OnRollback() again
        site_->OnRollback();
      } else {
        DCHECK(strcmp(command, "COMMIT") == 0);
        if (rv == SQLITE_OK) {
          site_->OnCommit();
        }
      }
      return rv;
    }
  } else {
    if (strcmp(command, "ROLLBACK") == 0) {
      site_->GetTopTransaction()->needs_rollback_ = true;
    }
    began_ = false;
    return SQLITE_OK;
  }
}

int SQLStatement::prepare(sqlite3* db, const char* sql, int sql_len) {
  DCHECK(!stmt_);
  int rv = sqlite3_prepare_v2(db, sql, sql_len, &stmt_, NULL);
  if (rv != SQLITE_OK) {
    DLOG(ERROR) << "SQLStatement.prepare_v2 failed: " << sqlite3_errmsg(db);
  }
  return rv;
}

int SQLStatement::step() {
  DCHECK(stmt_);
  return sqlite3_step(stmt_);
}

int SQLStatement::reset() {
  DCHECK(stmt_);
  return sqlite3_reset(stmt_);
}

sqlite_int64 SQLStatement::last_insert_rowid() {
  DCHECK(stmt_);
  return sqlite3_last_insert_rowid(db_handle());
}

sqlite3* SQLStatement::db_handle() {
  DCHECK(stmt_);
  return sqlite3_db_handle(stmt_);
}

int SQLStatement::bind_parameter_count() {
  DCHECK(stmt_);
  return sqlite3_bind_parameter_count(stmt_);
}

int SQLStatement::bind_blob(int index, std::vector<unsigned char>* blob) {
  if (blob) {
    const void* value = &(*blob)[0];
    int len = static_cast<int>(blob->size());
    return bind_blob(index, value, len);
  } else {
    return bind_null(index);
  }
}

int SQLStatement::bind_blob(int index, const void* value, int value_len) {
   return bind_blob(index, value, value_len, SQLITE_TRANSIENT);
}

int SQLStatement::bind_blob(int index, const void* value, int value_len,
                            Function dtor) {
  DCHECK(stmt_);
  return sqlite3_bind_blob(stmt_, index + 1, value, value_len, dtor);
}

int SQLStatement::bind_double(int index, double value) {
  DCHECK(stmt_);
  return sqlite3_bind_double(stmt_, index + 1, value);
}

int SQLStatement::bind_bool(int index, bool value) {
  DCHECK(stmt_);
  return sqlite3_bind_int(stmt_, index + 1, value);
}

int SQLStatement::bind_int(int index, int value) {
  DCHECK(stmt_);
  return sqlite3_bind_int(stmt_, index + 1, value);
}

int SQLStatement::bind_int64(int index, sqlite_int64 value) {
  DCHECK(stmt_);
  return sqlite3_bind_int64(stmt_, index + 1, value);
}

int SQLStatement::bind_null(int index) {
  DCHECK(stmt_);
  return sqlite3_bind_null(stmt_, index + 1);
}

int SQLStatement::bind_text(int index, const char* value, int value_len,
              Function dtor) {
  DCHECK(stmt_);
  return sqlite3_bind_text(stmt_, index + 1, value, value_len, dtor);
}

int SQLStatement::bind_text16(int index, const char16* value, int value_len,
                Function dtor) {
  DCHECK(stmt_);
  value_len *= sizeof(char16);
  return sqlite3_bind_text16(stmt_, index + 1, value, value_len, dtor);
}

int SQLStatement::bind_value(int index, const sqlite3_value* value) {
  DCHECK(stmt_);
  return sqlite3_bind_value(stmt_, index + 1, value);
}

int SQLStatement::column_count() {
  DCHECK(stmt_);
  return sqlite3_column_count(stmt_);
}

int SQLStatement::column_type(int index) {
  DCHECK(stmt_);
  return sqlite3_column_type(stmt_, index);
}

const void* SQLStatement::column_blob(int index) {
  DCHECK(stmt_);
  return sqlite3_column_blob(stmt_, index);
}

bool SQLStatement::column_blob_as_vector(int index,
                                         std::vector<unsigned char>* blob) {
  DCHECK(stmt_);
  const void* p = column_blob(index);
  size_t len = column_bytes(index);
  blob->resize(len);
  if (blob->size() != len) {
    return false;
  }
  if (len > 0)
    memcpy(&(blob->front()), p, len);
  return true;
}

bool SQLStatement::column_blob_as_string(int index, std::string* blob) {
  DCHECK(stmt_);
  const void* p = column_blob(index);
  size_t len = column_bytes(index);
  blob->resize(len);
  if (blob->size() != len) {
    return false;
  }
  blob->assign(reinterpret_cast<const char*>(p), len);
  return true;
}

int SQLStatement::column_bytes(int index) {
  DCHECK(stmt_);
  return sqlite3_column_bytes(stmt_, index);
}

int SQLStatement::column_bytes16(int index) {
  DCHECK(stmt_);
  return sqlite3_column_bytes16(stmt_, index);
}

double SQLStatement::column_double(int index) {
  DCHECK(stmt_);
  return sqlite3_column_double(stmt_, index);
}

bool SQLStatement::column_bool(int index) {
  DCHECK(stmt_);
  return sqlite3_column_int(stmt_, index) ? true : false;
}

int SQLStatement::column_int(int index) {
  DCHECK(stmt_);
  return sqlite3_column_int(stmt_, index);
}

sqlite_int64 SQLStatement::column_int64(int index) {
  DCHECK(stmt_);
  return sqlite3_column_int64(stmt_, index);
}

const char* SQLStatement::column_text(int index) {
  DCHECK(stmt_);
  return reinterpret_cast<const char*>(sqlite3_column_text(stmt_, index));
}

bool SQLStatement::column_string(int index, std::string* str) {
  DCHECK(stmt_);
  DCHECK(str);
  const char* s = column_text(index);
  str->assign(s ? s : std::string());
  return s != NULL;
}

std::string SQLStatement::column_string(int index) {
  std::string str;
  column_string(index, &str);
  return str;
}

const char16* SQLStatement::column_text16(int index) {
  DCHECK(stmt_);
  return static_cast<const char16*>(sqlite3_column_text16(stmt_, index));
}

bool SQLStatement::column_wstring(int index, std::wstring* str) {
  DCHECK(stmt_);
  DCHECK(str);
  const char* s = column_text(index);
  str->assign(s ? UTF8ToWide(s) : std::wstring());
  return (s != NULL);
}

std::wstring SQLStatement::column_wstring(int index) {
  std::wstring wstr;
  column_wstring(index, &wstr);
  return wstr;
}
