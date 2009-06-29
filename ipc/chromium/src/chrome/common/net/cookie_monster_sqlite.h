// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A sqlite implementation of a cookie monster persistent store.

#ifndef CHROME_COMMON_NET_COOKIE_MONSTER_SQLITE_H__
#define CHROME_COMMON_NET_COOKIE_MONSTER_SQLITE_H__

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/message_loop.h"
#include "chrome/browser/meta_table_helper.h"
#include "net/base/cookie_monster.h"

struct sqlite3;

class SQLitePersistentCookieStore
    : public net::CookieMonster::PersistentCookieStore {
 public:
  SQLitePersistentCookieStore(const std::wstring& path,
                              MessageLoop* background_loop);
  ~SQLitePersistentCookieStore();

  virtual bool Load(std::vector<net::CookieMonster::KeyedCanonicalCookie>*);

  virtual void AddCookie(const std::string&,
                         const net::CookieMonster::CanonicalCookie&);
  virtual void UpdateCookieAccessTime(
      const net::CookieMonster::CanonicalCookie&);
  virtual void DeleteCookie(const net::CookieMonster::CanonicalCookie&);

 private:
  class Backend;

  // Database upgrade statements.
  bool EnsureDatabaseVersion(sqlite3* db);

  std::wstring path_;
  scoped_refptr<Backend> backend_;

  // Background MessageLoop on which to access the backend_;
  MessageLoop* background_loop_;

  MetaTableHelper meta_table_;

  DISALLOW_EVIL_CONSTRUCTORS(SQLitePersistentCookieStore);
};

#endif  // CHROME_COMMON_NET_COOKIE_MONSTER_SQLITE_H__
