/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_indexeddb_schemaupgrades_h__
#define dom_indexeddb_schemaupgrades_h__

#include <cstdint>

#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "nsStringFwd.h"

class mozIStorageConnection;
class nsIFile;

namespace mozilla::dom::indexedDB {

Result<bool, nsresult> MaybeUpgradeSchema(mozIStorageConnection& aConnection,
                                          int32_t aSchemaVersion,
                                          nsIFile& aFMDirectory,
                                          const nsACString& aOrigin);

}  // namespace mozilla::dom::indexedDB

#endif
