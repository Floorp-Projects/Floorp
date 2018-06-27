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

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaKeyStatusMap)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaKeyStatusMap)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaKeyStatusMap)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaKeyStatusMap, mParent)

MediaKeyStatusMap::MediaKeyStatusMap(nsPIDOMWindowInner* aParent)
  : mParent(aParent)
{
}

MediaKeyStatusMap::~MediaKeyStatusMap()
{
}

JSObject*
MediaKeyStatusMap::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaKeyStatusMap_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner*
MediaKeyStatusMap::GetParentObject() const
{
  return mParent;
}

void
MediaKeyStatusMap::Get(JSContext* aCx,
                       const ArrayBufferViewOrArrayBuffer& aKey,
                       JS::MutableHandle<JS::Value> aOutValue,
                       ErrorResult& aOutRv) const
{
  ArrayData keyId = GetArrayBufferViewOrArrayBufferData(aKey);
  if (!keyId.IsValid()) {
    aOutValue.setUndefined();
    return;
  }
  for (const KeyStatus& status : mStatuses) {
    if (keyId == status.mKeyId) {
      bool ok = ToJSValue(aCx, status.mStatus, aOutValue);
      if (!ok) {
        aOutRv.NoteJSContextException(aCx);
      }
      return;
    }
  }
  aOutValue.setUndefined();
}

bool
MediaKeyStatusMap::Has(const ArrayBufferViewOrArrayBuffer& aKey) const
{
  ArrayData keyId = GetArrayBufferViewOrArrayBufferData(aKey);
  if (!keyId.IsValid()) {
    return false;
  }

  for (const KeyStatus& status : mStatuses) {
    if (keyId == status.mKeyId) {
      return true;
    }
  }

  return false;
}

uint32_t
MediaKeyStatusMap::GetIterableLength() const
{
  return mStatuses.Length();
}

TypedArrayCreator<ArrayBuffer>
MediaKeyStatusMap::GetKeyAtIndex(uint32_t aIndex) const
{
  MOZ_ASSERT(aIndex < GetIterableLength());
  return TypedArrayCreator<ArrayBuffer>(mStatuses[aIndex].mKeyId);
}

nsString
MediaKeyStatusMap::GetKeyIDAsHexString(uint32_t aIndex) const
{
  MOZ_ASSERT(aIndex < GetIterableLength());
  return NS_ConvertUTF8toUTF16(ToHexString(mStatuses[aIndex].mKeyId));
}

MediaKeyStatus
MediaKeyStatusMap::GetValueAtIndex(uint32_t aIndex) const
{
  MOZ_ASSERT(aIndex < GetIterableLength());
  return mStatuses[aIndex].mStatus;
}

uint32_t
MediaKeyStatusMap::Size() const
{
  return mStatuses.Length();
}

void
MediaKeyStatusMap::Update(const nsTArray<CDMCaps::KeyStatus>& aKeys)
{
  mStatuses.Clear();
  for (const auto& key : aKeys) {
    mStatuses.InsertElementSorted(KeyStatus(key.mId, key.mStatus));
  }
}

} // namespace dom
} // namespace mozilla
