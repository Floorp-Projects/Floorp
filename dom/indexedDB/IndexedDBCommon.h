/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_IndexedDBCommon_h
#define mozilla_dom_indexeddb_IndexedDBCommon_h

#include "mozilla/dom/quota/QuotaCommon.h"

// IndexedDB equivalent of QM_TRY.
#define IDB_TRY(...) QM_TRY_META(mozilla::dom::indexedDB, ##__VA_ARGS__)

// IndexedDB equivalent of QM_TRY_VAR.
#define IDB_TRY_VAR(...) QM_TRY_VAR_META(mozilla::dom::indexedDB, ##__VA_ARGS__)

namespace mozilla::dom::indexedDB {

void HandleError(const nsLiteralCString& aExpr,
                 const nsLiteralCString& aSourceFile, int32_t aSourceLine);

}  // namespace mozilla::dom::indexedDB

#endif  // mozilla_dom_indexeddb_IndexedDBCommon_h
