// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/sqlite_compiled_statement.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"

// SqliteStatementCache -------------------------------------------------------

SqliteStatementCache::~SqliteStatementCache() {
  STLDeleteContainerPairSecondPointers(statements_.begin(), statements_.end());
  statements_.clear();
  db_ = NULL;
}

void SqliteStatementCache::set_db(sqlite3* db) {
  DCHECK(!db_) << "Setting the database twice";
  db_ = db;
}

SQLStatement* SqliteStatementCache::InternalGetStatement(const char* func_name,
                                                         int func_number,
                                                         const char* sql) {
  FuncID id;
  id.name = func_name;
  id.number = func_number;

  StatementMap::const_iterator found = statements_.find(id);
  if (found != statements_.end())
    return found->second;

  if (!sql)
    return NULL;  // Don't create a new statement when we were not given SQL.

  // Create a new statement.
  SQLStatement* statement = new SQLStatement();
  if (statement->prepare(db_, sql) != SQLITE_OK) {
    const char* err_msg = sqlite3_errmsg(db_);
    NOTREACHED() << "SQL preparation error for: " << err_msg;
    return NULL;
  }

  statements_[id] = statement;
  return statement;
}

bool SqliteStatementCache::FuncID::operator<(const FuncID& other) const {
  // Compare numbers first since they are faster than strings and they are
  // almost always unique.
  if (number != other.number)
    return number < other.number;
  return name < other.name;
}

// SqliteCompiledStatement ----------------------------------------------------

SqliteCompiledStatement::SqliteCompiledStatement(const char* func_name,
                                                 int func_number,
                                                 SqliteStatementCache& cache,
                                                 const char* sql) {
  statement_ = cache.GetStatement(func_name, func_number, sql);
}

SqliteCompiledStatement::~SqliteCompiledStatement() {
  // Reset the statement so that subsequent callers don't get crap in it.
  if (statement_)
    statement_->reset();
}

SQLStatement& SqliteCompiledStatement::operator*() {
  DCHECK(statement_) << "Should check is_valid() before using the statement.";
  return *statement_;
}
SQLStatement* SqliteCompiledStatement::operator->() {
  DCHECK(statement_) << "Should check is_valid() before using the statement.";
  return statement_;
}
SQLStatement* SqliteCompiledStatement::statement() {
  DCHECK(statement_) << "Should check is_valid() before using the statement.";
  return statement_;
}
