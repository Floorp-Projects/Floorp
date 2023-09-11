/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaKeyStatusMap.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/EMEUtils.h"
#include "GMPUtils.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaKeyStatusMap)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaKeyStatusMap)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaKeyStatusMap)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaKeyStatusMap, mParent)

MediaKeyStatusMap::MediaKeyStatusMap(nsPIDOMWindowInner* aParent)
    : mParent(aParent) {}

MediaKeyStatusMap::~MediaKeyStatusMap() = default;

JSObject* MediaKeyStatusMap::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return MediaKeyStatusMap_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* MediaKeyStatusMap::GetParentObject() const {
  return mParent;
}

const MediaKeyStatusMap::KeyStatus* MediaKeyStatusMap::FindKey(
    const ArrayBufferViewOrArrayBuffer& aKey) const {
  MOZ_ASSERT(aKey.IsArrayBuffer() || aKey.IsArrayBufferView());

  return ProcessTypedArrays(aKey,
                            [&](const Span<const uint8_t>& aData,
                                JS::AutoCheckCannotGC&&) -> const KeyStatus* {
                              for (const KeyStatus& status : mStatuses) {
                                if (aData == Span(status.mKeyId)) {
                                  return &status;
                                }
                              }
                              return nullptr;
                            });
}

void MediaKeyStatusMap::Get(const ArrayBufferViewOrArrayBuffer& aKey,
                            OwningMediaKeyStatusOrUndefined& aOutValue,
                            ErrorResult& aOutRv) const {
  const KeyStatus* status = FindKey(aKey);
  if (!status) {
    aOutValue.SetUndefined();
    return;
  }

  aOutValue.SetAsMediaKeyStatus() = status->mStatus;
}

bool MediaKeyStatusMap::Has(const ArrayBufferViewOrArrayBuffer& aKey) const {
  return FindKey(aKey);
}

uint32_t MediaKeyStatusMap::GetIterableLength() const {
  return mStatuses.Length();
}

TypedArrayCreator<ArrayBuffer> MediaKeyStatusMap::GetKeyAtIndex(
    uint32_t aIndex) const {
  MOZ_ASSERT(aIndex < GetIterableLength());
  return TypedArrayCreator<ArrayBuffer>(mStatuses[aIndex].mKeyId);
}

nsString MediaKeyStatusMap::GetKeyIDAsHexString(uint32_t aIndex) const {
  MOZ_ASSERT(aIndex < GetIterableLength());
  return NS_ConvertUTF8toUTF16(ToHexString(mStatuses[aIndex].mKeyId));
}

MediaKeyStatus MediaKeyStatusMap::GetValueAtIndex(uint32_t aIndex) const {
  MOZ_ASSERT(aIndex < GetIterableLength());
  return mStatuses[aIndex].mStatus;
}

uint32_t MediaKeyStatusMap::Size() const { return mStatuses.Length(); }

void MediaKeyStatusMap::Update(const nsTArray<CDMCaps::KeyStatus>& aKeys) {
  mStatuses.Clear();
  for (const auto& key : aKeys) {
    mStatuses.InsertElementSorted(KeyStatus(key.mId, key.mStatus));
  }
}

}  // namespace mozilla::dom
