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

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaKeyStatusMap)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaKeyStatusMap)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaKeyStatusMap)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaKeyStatusMap, mParent)

MediaKeyStatusMap::MediaKeyStatusMap(nsPIDOMWindow* aParent)
  : mParent(aParent)
{
}

MediaKeyStatusMap::~MediaKeyStatusMap()
{
}

JSObject*
MediaKeyStatusMap::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaKeyStatusMapBinding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindow*
MediaKeyStatusMap::GetParentObject() const
{
  return mParent;
}

MediaKeyStatus
MediaKeyStatusMap::Get(const ArrayBufferViewOrArrayBuffer& aKey) const
{
  ArrayData keyId = GetArrayBufferViewOrArrayBufferData(aKey);
  if (!keyId.IsValid()) {
    return MediaKeyStatus::Internal_error;
  }

  for (const KeyStatus& status : mStatuses) {
    if (keyId == status.mKeyId) {
      return status.mStatus;
    }
  }

  return MediaKeyStatus::Internal_error;
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

static MediaKeyStatus
ToMediaKeyStatus(GMPMediaKeyStatus aStatus) {
  switch (aStatus) {
    case kGMPUsable: return MediaKeyStatus::Usable;
    case kGMPExpired: return MediaKeyStatus::Expired;
    case kGMPOutputDownscaled: return MediaKeyStatus::Output_downscaled;
    case kGMPOutputNotAllowed: return MediaKeyStatus::Output_not_allowed;
    case kGMPInternalError: return MediaKeyStatus::Internal_error;
    default: return MediaKeyStatus::Internal_error;
  }
}

void
MediaKeyStatusMap::Update(const nsTArray<CDMCaps::KeyStatus>& aKeys)
{
  mStatuses.Clear();
  for (const auto& key : aKeys) {
    mStatuses.InsertElementSorted(KeyStatus(key.mId, ToMediaKeyStatus(key.mStatus)));
  }
}

} // namespace dom
} // namespace mozilla
