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

// XXX Replace all uses by the QM_* variants and remove these aliases
#define IDB_TRY QM_TRY
#define IDB_TRY_UNWRAP QM_TRY_UNWRAP
#define IDB_TRY_INSPECT QM_TRY_INSPECT
#define IDB_TRY_RETURN QM_TRY_RETURN
#define IDB_FAIL QM_FAIL

namespace mozilla::dom::indexedDB {

static constexpr uint32_t kFileCopyBufferSize = 32768;

nsresult SnappyUncompressStructuredCloneData(
    nsIInputStream& aInputStream, JSStructuredCloneData& aStructuredCloneData);

}  // namespace mozilla::dom::indexedDB

#endif  // mozilla_dom_indexeddb_IndexedDBCommon_h
