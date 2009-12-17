// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/cookie_monster_sqlite.h"

#include <list>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/sqlite_utils.h"

using base::Time;

// This class is designed to be shared between any calling threads and the
// database thread.  It batches operations and commits them on a timer.
class SQLitePersistentCookieStore::Backend
    : public base::RefCountedThreadSafe<SQLitePersistentCookieStore::Backend> {
 public:
  // The passed database pointer must be already-initialized. This object will
  // take ownership.
  explicit Backend(sqlite3* db, MessageLoop* loop)
      : db_(db),
        background_loop_(loop),
        cache_(new SqliteStatementCache(db)),
        num_pending_(0) {
    DCHECK(db_) << "Database must exist.";
  }

  // You should call Close() before destructing this object.
  ~Backend() {
    DCHECK(!db_) << "Close should have already been called.";
    DCHECK(num_pending_ == 0 && pending_.empty());
  }

  // Batch a cookie addition.
  void AddCookie(const std::string& key,
                 const net::CookieMonster::CanonicalCookie& cc);

  // Batch a cookie access time update.
  void UpdateCookieAccessTime(const net::CookieMonster::CanonicalCookie& cc);

  // Batch a cookie deletion.
  void DeleteCookie(const net::CookieMonster::CanonicalCookie& cc);

  // Commit any pending operations and close the database.  This must be called
  // before the object is destructed.
  void Close();

 private:
  class PendingOperation {
   public:
    typedef enum {
      COOKIE_ADD,
      COOKIE_UPDATEACCESS,
      COOKIE_DELETE,
    } OperationType;

    PendingOperation(OperationType op,
                     const std::string& key,
                     const net::CookieMonster::CanonicalCookie& cc)
        : op_(op), key_(key), cc_(cc) { }

    OperationType op() const { return op_; }
    const std::string& key() const { return key_; }
    const net::CookieMonster::CanonicalCookie& cc() const { return cc_; }

   private:
    OperationType op_;
    std::string key_;  // Only used for OP_ADD
    net::CookieMonster::CanonicalCookie cc_;
  };

 private:
  // Batch a cookie operation (add or delete)
  void BatchOperation(PendingOperation::OperationType op,
                      const std::string& key,
                      const net::CookieMonster::CanonicalCookie& cc);
  // Commit our pending operations to the database.
  void Commit();
  // Close() executed on the background thread.
  void InternalBackgroundClose();

  sqlite3* db_;
  MessageLoop* background_loop_;
  SqliteStatementCache* cache_;

  typedef std::list<PendingOperation*> PendingOperationsList;
  PendingOperationsList pending_;
  PendingOperationsList::size_type num_pending_;
  Lock pending_lock_;  // Guard pending_ and num_pending_

  DISALLOW_EVIL_CONSTRUCTORS(Backend);
};

void SQLitePersistentCookieStore::Backend::AddCookie(
    const std::string& key,
    const net::CookieMonster::CanonicalCookie& cc) {
  BatchOperation(PendingOperation::COOKIE_ADD, key, cc);
}

void SQLitePersistentCookieStore::Backend::UpdateCookieAccessTime(
    const net::CookieMonster::CanonicalCookie& cc) {
  BatchOperation(PendingOperation::COOKIE_UPDATEACCESS, std::string(), cc);
}

void SQLitePersistentCookieStore::Backend::DeleteCookie(
    const net::CookieMonster::CanonicalCookie& cc) {
  BatchOperation(PendingOperation::COOKIE_DELETE, std::string(), cc);
}

void SQLitePersistentCookieStore::Backend::BatchOperation(
    PendingOperation::OperationType op,
    const std::string& key,
    const net::CookieMonster::CanonicalCookie& cc) {
  // Commit every 30 seconds.
  static const int kCommitIntervalMs = 30 * 1000;
  // Commit right away if we have more than 512 outstanding operations.
  static const size_t kCommitAfterBatchSize = 512;
  DCHECK(MessageLoop::current() != background_loop_);

  // We do a full copy of the cookie here, and hopefully just here.
  scoped_ptr<PendingOperation> po(new PendingOperation(op, key, cc));
  CHECK(po.get());

  PendingOperationsList::size_type num_pending;
  {
    AutoLock locked(pending_lock_);
    pending_.push_back(po.release());
    num_pending = ++num_pending_;
  }

  // TODO(abarth): What if the DB thread is being destroyed on the UI thread?
  if (num_pending == 1) {
    // We've gotten our first entry for this batch, fire off the timer.
    background_loop_->PostDelayedTask(FROM_HERE,
        NewRunnableMethod(this, &Backend::Commit), kCommitIntervalMs);
  } else if (num_pending == kCommitAfterBatchSize) {
    // We've reached a big enough batch, fire off a commit now.
    background_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &Backend::Commit));
  }
}

void SQLitePersistentCookieStore::Backend::Commit() {
  DCHECK(MessageLoop::current() == background_loop_);
  PendingOperationsList ops;
  {
    AutoLock locked(pending_lock_);
    pending_.swap(ops);
    num_pending_ = 0;
  }

  // Maybe an old timer fired or we are already Close()'ed.
  if (!db_ || ops.empty())
    return;

  SQLITE_UNIQUE_STATEMENT(add_smt, *cache_,
      "INSERT INTO cookies (creation_utc, host_key, name, value, path, "
      "expires_utc, secure, httponly, last_access_utc) "
      "VALUES (?,?,?,?,?,?,?,?,?)");
  if (!add_smt.is_valid()) {
    NOTREACHED();
    return;
  }

  SQLITE_UNIQUE_STATEMENT(update_access_smt, *cache_,
                          "UPDATE cookies SET last_access_utc=? "
                          "WHERE creation_utc=?");
  if (!update_access_smt.is_valid()) {
    NOTREACHED();
    return;
  }

  SQLITE_UNIQUE_STATEMENT(del_smt, *cache_,
                          "DELETE FROM cookies WHERE creation_utc=?");
  if (!del_smt.is_valid()) {
    NOTREACHED();
    return;
  }

  SQLTransaction transaction(db_);
  transaction.Begin();
  for (PendingOperationsList::iterator it = ops.begin();
       it != ops.end(); ++it) {
    // Free the cookies as we commit them to the database.
    scoped_ptr<PendingOperation> po(*it);
    switch (po->op()) {
      case PendingOperation::COOKIE_ADD:
        add_smt->reset();
        add_smt->bind_int64(0, po->cc().CreationDate().ToInternalValue());
        add_smt->bind_string(1, po->key());
        add_smt->bind_string(2, po->cc().Name());
        add_smt->bind_string(3, po->cc().Value());
        add_smt->bind_string(4, po->cc().Path());
        add_smt->bind_int64(5, po->cc().ExpiryDate().ToInternalValue());
        add_smt->bind_int(6, po->cc().IsSecure());
        add_smt->bind_int(7, po->cc().IsHttpOnly());
        add_smt->bind_int64(8, po->cc().LastAccessDate().ToInternalValue());
        if (add_smt->step() != SQLITE_DONE) {
          NOTREACHED() << "Could not add a cookie to the DB.";
        }
        break;

      case PendingOperation::COOKIE_UPDATEACCESS:
        update_access_smt->reset();
        update_access_smt->bind_int64(0,
            po->cc().LastAccessDate().ToInternalValue());
        update_access_smt->bind_int64(1,
            po->cc().CreationDate().ToInternalValue());
        if (update_access_smt->step() != SQLITE_DONE) {
          NOTREACHED() << "Could not update cookie last access time in the DB.";
        }
        break;

      case PendingOperation::COOKIE_DELETE:
        del_smt->reset();
        del_smt->bind_int64(0, po->cc().CreationDate().ToInternalValue());
        if (del_smt->step() != SQLITE_DONE) {
          NOTREACHED() << "Could not delete a cookie from the DB.";
        }
        break;

      default:
        NOTREACHED();
        break;
    }
  }
  transaction.Commit();
}

// Fire off a close message to the background thread.  We could still have a
// pending commit timer that will be holding a reference on us, but if/when
// this fires we will already have been cleaned up and it will be ignored.
void SQLitePersistentCookieStore::Backend::Close() {
  DCHECK(MessageLoop::current() != background_loop_);
  // Must close the backend on the background thread.
  // TODO(abarth): What if the DB thread is being destroyed on the UI thread?
  background_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &Backend::InternalBackgroundClose));
}

void SQLitePersistentCookieStore::Backend::InternalBackgroundClose() {
  DCHECK(MessageLoop::current() == background_loop_);
  // Commit any pending operations
  Commit();
  // We must destroy the cache before closing the database.
  delete cache_;
  cache_ = NULL;
  sqlite3_close(db_);
  db_ = NULL;
}

SQLitePersistentCookieStore::SQLitePersistentCookieStore(
    const std::wstring& path, MessageLoop* background_loop)
    : path_(path),
      background_loop_(background_loop) {
  DCHECK(background_loop) << "SQLitePersistentCookieStore needs a MessageLoop";
}

SQLitePersistentCookieStore::~SQLitePersistentCookieStore() {
  if (backend_.get()) {
    backend_->Close();
    // Release our reference, it will probably still have a reference if the
    // background thread has not run Close() yet.
    backend_ = NULL;
  }
}

// Version number of the database.
static const int kCurrentVersionNumber = 3;
static const int kCompatibleVersionNumber = 3;

namespace {

// Initializes the cookies table, returning true on success.
bool InitTable(sqlite3* db) {
  if (!DoesSqliteTableExist(db, "cookies")) {
    if (sqlite3_exec(db, "CREATE TABLE cookies ("
                         "creation_utc INTEGER NOT NULL UNIQUE PRIMARY KEY,"
                         "host_key TEXT NOT NULL,"
                         "name TEXT NOT NULL,"
                         "value TEXT NOT NULL,"
                         "path TEXT NOT NULL,"
                         // We only store persistent, so we know it expires
                         "expires_utc INTEGER NOT NULL,"
                         "secure INTEGER NOT NULL,"
                         "httponly INTEGER NOT NULL,"
                         "last_access_utc INTEGER NOT NULL)",
                         NULL, NULL, NULL) != SQLITE_OK)
      return false;
  }

  // Try to create the index every time. Older versions did not have this index,
  // so we want those people to get it. Ignore errors, since it may exist.
  sqlite3_exec(db, "CREATE INDEX cookie_times ON cookies (creation_utc)",
               NULL, NULL, NULL);

  return true;
}

void PrimeCache(sqlite3* db) {
  // A statement must be open for the preload command to work. If the meta
  // table can't be read, it probably means this is a new database and there
  // is nothing to preload (so it's OK we do nothing).
  SQLStatement dummy;
  if (dummy.prepare(db, "SELECT * from meta") != SQLITE_OK)
    return;
  if (dummy.step() != SQLITE_ROW)
    return;

  sqlite3Preload(db);
}

}  // namespace

bool SQLitePersistentCookieStore::Load(
    std::vector<net::CookieMonster::KeyedCanonicalCookie>* cookies) {
  DCHECK(!path_.empty());
  sqlite3* db;
  if (sqlite3_open(WideToUTF8(path_).c_str(), &db) != SQLITE_OK) {
    NOTREACHED() << "Unable to open cookie DB.";
    return false;
  }

  if (!EnsureDatabaseVersion(db) || !InitTable(db)) {
    NOTREACHED() << "Unable to initialize cookie DB.";
    sqlite3_close(db);
    return false;
  }

  PrimeCache(db);

  // Slurp all the cookies into the out-vector.
  SQLStatement smt;
  if (smt.prepare(db,
      "SELECT creation_utc, host_key, name, value, path, expires_utc, secure, "
      "httponly, last_access_utc FROM cookies") != SQLITE_OK) {
    NOTREACHED() << "select statement prep failed";
    sqlite3_close(db);
    return false;
  }

  while (smt.step() == SQLITE_ROW) {
    std::string key = smt.column_string(1);
    scoped_ptr<net::CookieMonster::CanonicalCookie> cc(
        new net::CookieMonster::CanonicalCookie(
            smt.column_string(2),                            // name
            smt.column_string(3),                            // value
            smt.column_string(4),                            // path
            smt.column_int(6) != 0,                          // secure
            smt.column_int(7) != 0,                          // httponly
            Time::FromInternalValue(smt.column_int64(0)),    // creation_utc
            Time::FromInternalValue(smt.column_int64(8)),    // last_access_utc
            true,                                            // has_expires
            Time::FromInternalValue(smt.column_int64(5))));  // expires_utc
    // Memory allocation failed.
    if (!cc.get())
      break;

    DLOG_IF(WARNING,
            cc->CreationDate() > Time::Now()) << L"CreationDate too recent";
    cookies->push_back(
        net::CookieMonster::KeyedCanonicalCookie(smt.column_string(1),
                                                 cc.release()));
  }

  // Create the backend, this will take ownership of the db pointer.
  backend_ = new Backend(db, background_loop_);

  return true;
}

bool SQLitePersistentCookieStore::EnsureDatabaseVersion(sqlite3* db) {
  // Version check.
  if (!meta_table_.Init(std::string(), kCurrentVersionNumber,
                        kCompatibleVersionNumber, db))
    return false;

  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Cookie database is too new.";
    return false;
  }

  int cur_version = meta_table_.GetVersionNumber();
  if (cur_version == 2) {
    SQLTransaction transaction(db);
    transaction.Begin();
    if ((sqlite3_exec(db,
                      "ALTER TABLE cookies ADD COLUMN last_access_utc "
                      "INTEGER DEFAULT 0", NULL, NULL, NULL) != SQLITE_OK) ||
        (sqlite3_exec(db,
                      "UPDATE cookies SET last_access_utc = creation_utc",
                      NULL, NULL, NULL) != SQLITE_OK)) {
      LOG(WARNING) << "Unable to update cookie database to version 3.";
      return false;
    }
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
    meta_table_.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
    transaction.Commit();
  }

  // Put future migration cases here.

  // When the version is too old, we just try to continue anyway, there should
  // not be a released product that makes a database too old for us to handle.
  LOG_IF(WARNING, cur_version < kCurrentVersionNumber) <<
      "Cookie database version " << cur_version << " is too old to handle.";

  return true;
}

void SQLitePersistentCookieStore::AddCookie(
    const std::string& key,
    const net::CookieMonster::CanonicalCookie& cc) {
  if (backend_.get())
    backend_->AddCookie(key, cc);
}

void SQLitePersistentCookieStore::UpdateCookieAccessTime(
    const net::CookieMonster::CanonicalCookie& cc) {
  if (backend_.get())
    backend_->UpdateCookieAccessTime(cc);
}

void SQLitePersistentCookieStore::DeleteCookie(
    const net::CookieMonster::CanonicalCookie& cc) {
  if (backend_.get())
    backend_->DeleteCookie(cc);
}
