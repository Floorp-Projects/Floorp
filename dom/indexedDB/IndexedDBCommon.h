/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_IndexedDBCommon_h
#define mozilla_dom_indexeddb_IndexedDBCommon_h

#include "mozilla/dom/quota/QuotaCommon.h"

class JSStructuredCloneData;
class nsIInputStream;

// IndexedDB equivalents of QM_TRY.
#define IDB_TRY_GLUE(...) \
  QM_TRY_META(mozilla::dom::indexedDB, MOZ_UNIQUE_VAR(tryResult), ##__VA_ARGS__)
#define IDB_TRY(...) IDB_TRY_GLUE(__VA_ARGS__)

// IndexedDB equivalents of QM_TRY_UNWRAP and QM_TRY_INSPECT.
#define IDB_TRY_ASSIGN_GLUE(accessFunction, ...)                         \
  QM_TRY_ASSIGN_META(mozilla::dom::indexedDB, MOZ_UNIQUE_VAR(tryResult), \
                     accessFunction, ##__VA_ARGS__)
#define IDB_TRY_UNWRAP(...) IDB_TRY_ASSIGN_GLUE(unwrap, __VA_ARGS__)
#define IDB_TRY_INSPECT(...) IDB_TRY_ASSIGN_GLUE(inspect, __VA_ARGS__)

// IndexedDB equivalents of QM_TRY_RETURN.
#define IDB_TRY_RETURN_GLUE(...)                                         \
  QM_TRY_RETURN_META(mozilla::dom::indexedDB, MOZ_UNIQUE_VAR(tryResult), \
                     ##__VA_ARGS__)
#define IDB_TRY_RETURN(...) IDB_TRY_RETURN_GLUE(__VA_ARGS__)

// IndexedDB equivalents of QM_FAIL.
#define IDB_FAIL_GLUE(...) QM_FAIL_META(mozilla::dom::indexedDB, ##__VA_ARGS__)
#define IDB_FAIL(...) IDB_FAIL_GLUE(__VA_ARGS__)

namespace mozilla::dom::indexedDB {

QM_META_HANDLE_ERROR("IndexedDB"_ns)

static constexpr uint32_t kFileCopyBufferSize = 32768;

nsresult SnappyUncompressStructuredCloneData(
    nsIInputStream& aInputStream, JSStructuredCloneData& aStructuredCloneData);

}  // namespace mozilla::dom::indexedDB

#endif  // mozilla_dom_indexeddb_IndexedDBCommon_h
