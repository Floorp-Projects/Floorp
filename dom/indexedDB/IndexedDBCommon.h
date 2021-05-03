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

namespace mozilla::dom::indexedDB {

static constexpr uint32_t kFileCopyBufferSize = 32768;

nsresult SnappyUncompressStructuredCloneData(
    nsIInputStream& aInputStream, JSStructuredCloneData& aStructuredCloneData);

}  // namespace mozilla::dom::indexedDB

#endif  // mozilla_dom_indexeddb_IndexedDBCommon_h
