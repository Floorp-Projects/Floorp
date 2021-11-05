/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_QUOTADATABASEHELPER_H
#define DOM_QUOTA_QUOTADATABASEHELPER_H

#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozIStorageConnection.h"
#include "nsCOMPtr.h"
#include "nsString.h"

namespace mozilla::dom::quota {

/**
 * This class provides a RAII wrap of attaching and detaching database
 * in a given C++ scope. It is guaranteed that an attached database will
 * be detached even if you have an exception or return early.
 *
 * @param aConnection
 *        The connection to attach a database to.
 * @param aDatabaseFile
 *        The database file to attach.
 * @param aSchemaName
 *        The schema-name. Can be any string literal which is supported by the
 *        underlying database. For more details about schema-name, see
 *        https://www.sqlite.org/lang_attach.html
 */
class MOZ_STACK_CLASS AutoDatabaseAttacher final {
 public:
  explicit AutoDatabaseAttacher(nsCOMPtr<mozIStorageConnection> aConnection,
                                nsCOMPtr<nsIFile> aDatabaseFile,
                                const nsLiteralCString& aSchemaName);

  ~AutoDatabaseAttacher();

  AutoDatabaseAttacher() = delete;

  [[nodiscard]] nsresult Attach();

  [[nodiscard]] nsresult Detach();

 private:
  nsCOMPtr<mozIStorageConnection> mConnection;
  nsCOMPtr<nsIFile> mDatabaseFile;
  const nsLiteralCString mSchemaName;
  bool mAttached;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_QUOTADATABASEHELPER_H
