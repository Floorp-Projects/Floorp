/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_indexeddb_dbschema_h__
#define dom_indexeddb_dbschema_h__

#include "ErrorList.h"

#include <cstdint>

class mozIStorageConnection;

namespace mozilla::dom::indexedDB {

// Major schema version. Bump for almost everything.
const uint32_t kMajorSchemaVersion = 26;

// Minor schema version. Should almost always be 0 (maybe bump on release
// branches if we have to).
const uint32_t kMinorSchemaVersion = 0;

// The schema version we store in the SQLite database is a (signed) 32-bit
// integer. The major version is left-shifted 4 bits so the max value is
// 0xFFFFFFF. The minor version occupies the lower 4 bits and its max is 0xF.
static_assert(kMajorSchemaVersion <= 0xFFFFFFF,
              "Major version needs to fit in 28 bits.");
static_assert(kMinorSchemaVersion <= 0xF,
              "Minor version needs to fit in 4 bits.");

constexpr int32_t MakeSchemaVersion(uint32_t aMajorSchemaVersion,
                                    uint32_t aMinorSchemaVersion) {
  return int32_t((aMajorSchemaVersion << 4) + aMinorSchemaVersion);
}

constexpr int32_t kSQLiteSchemaVersion =
    MakeSchemaVersion(kMajorSchemaVersion, kMinorSchemaVersion);

nsresult CreateFileTables(mozIStorageConnection& aConnection);
nsresult CreateTables(mozIStorageConnection& aConnection);

}  // namespace mozilla::dom::indexedDB

#endif
