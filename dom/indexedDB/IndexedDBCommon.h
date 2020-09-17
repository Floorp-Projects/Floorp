/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_IndexedDBCommon_h
#define mozilla_dom_indexeddb_IndexedDBCommon_h

#include "mozilla/dom/quota/QuotaCommon.h"

// IndexedDB equivalents of QM_TRY and QM_DEBUG_TRY.
#define IDB_TRY_GLUE(...) \
  QM_TRY_META(mozilla::dom::indexedDB, MOZ_UNIQUE_VAR(tryResult), ##__VA_ARGS__)
#define IDB_TRY(...) IDB_TRY_GLUE(__VA_ARGS__)

#ifdef DEBUG
#  define IDB_DEBUG_TRY(...) IDB_TRY(__VA_ARGS__)
#else
#  define IDB_DEBUG_TRY(...)
#endif

// IndexedDB equivalents of QM_TRY_VAR and QM_DEBUG_TRY_VAR.
#define IDB_TRY_VAR_GLUE(...)                                         \
  QM_TRY_VAR_META(mozilla::dom::indexedDB, MOZ_UNIQUE_VAR(tryResult), \
                  ##__VA_ARGS__)
#define IDB_TRY_VAR(...) IDB_TRY_VAR_GLUE(__VA_ARGS__)

#ifdef DEBUG
#  define IDB_DEBUG_TRY_VAR(...) IDB_TRY_VAR(__VA_ARGS__)
#else
#  define IDB_DEBUG_TRY_VAR(...)
#endif

// IndexedDB equivalents of QM_TRY_RETURN and QM_DEBUG_TRY_RETURN.
#define IDB_TRY_RETURN_GLUE(...)                                         \
  QM_TRY_RETURN_META(mozilla::dom::indexedDB, MOZ_UNIQUE_VAR(tryResult), \
                     ##__VA_ARGS__)
#define IDB_TRY_RETURN(...) IDB_TRY_RETURN_GLUE(__VA_ARGS__)

#ifdef DEBUG
#  define IDB_DEBUG_TRY_RETURN(...) IDB_TRY_RETURN(__VA_ARGS__)
#else
#  define IDB_DEBUG_TRY_RETURN(...)
#endif

// IndexedDB equivalents of QM_FAIL and QM_DEBUG_FAIL.
#define IDB_FAIL_GLUE(...) QM_FAIL_META(mozilla::dom::indexedDB, ##__VA_ARGS__)
#define IDB_FAIL(...) IDB_FAIL_GLUE(__VA_ARGS__)

#ifdef DEBUG
#  define IDB_DEBUG_FAIL(...) IDB_FAIL(__VA_ARGS__)
#else
#  define IDB_DEBUG_FAIL(...)
#endif

namespace mozilla::dom::indexedDB {

QM_META_HANDLE_ERROR("IndexedDB"_ns)

}  // namespace mozilla::dom::indexedDB

#endif  // mozilla_dom_indexeddb_IndexedDBCommon_h
