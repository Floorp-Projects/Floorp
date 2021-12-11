/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBCursorType.h"

#include "IndexedDatabaseInlines.h"

namespace mozilla::dom {
CommonCursorDataBase::CommonCursorDataBase(Key aKey) : mKey{std::move(aKey)} {}

IndexCursorDataBase::IndexCursorDataBase(Key aKey, Key aLocaleAwareKey,
                                         Key aObjectStoreKey)
    : CommonCursorDataBase{std::move(aKey)},
      mLocaleAwareKey{std::move(aLocaleAwareKey)},
      mObjectStoreKey{std::move(aObjectStoreKey)} {}

ValueCursorDataBase::ValueCursorDataBase(
    StructuredCloneReadInfoChild&& aCloneInfo)
    : mCloneInfo{std::move(aCloneInfo)} {}

CursorData<IDBCursorType::ObjectStore>::CursorData(
    Key aKey, StructuredCloneReadInfoChild&& aCloneInfo)
    : ObjectStoreCursorDataBase{std::move(aKey)},
      ValueCursorDataBase{std::move(aCloneInfo)} {}

CursorData<IDBCursorType::Index>::CursorData(
    Key aKey, Key aLocaleAwareKey, Key aObjectStoreKey,
    StructuredCloneReadInfoChild&& aCloneInfo)
    : IndexCursorDataBase{std::move(aKey), std::move(aLocaleAwareKey),
                          std::move(aObjectStoreKey)},
      ValueCursorDataBase{std::move(aCloneInfo)} {}

}  // namespace mozilla::dom
