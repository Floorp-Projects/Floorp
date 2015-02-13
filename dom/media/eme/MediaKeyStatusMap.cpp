/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaKeyStatusMap.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/dom/ToJSValue.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaKeyStatusMap)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaKeyStatusMap)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaKeyStatusMap)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaKeyStatusMap)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MediaKeyStatusMap)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mMap = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(MediaKeyStatusMap)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(MediaKeyStatusMap)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mMap)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

MediaKeyStatusMap::MediaKeyStatusMap(JSContext* aCx,
                                     nsPIDOMWindow* aParent,
                                     ErrorResult& aRv)
  : mParent(aParent)
  , mUpdateError(NS_OK)
{
  mMap = JS::NewMapObject(aCx);
  if (NS_WARN_IF(!mMap)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
  }

  mozilla::HoldJSObjects(this);
}

MediaKeyStatusMap::~MediaKeyStatusMap()
{
  mozilla::DropJSObjects(this);
}

JSObject*
MediaKeyStatusMap::WrapObject(JSContext* aCx)
{
  return MediaKeyStatusMapBinding::Wrap(aCx, this);
}

nsPIDOMWindow*
MediaKeyStatusMap::GetParentObject() const
{
  return mParent;
}

MediaKeyStatus
MediaKeyStatusMap::Get(JSContext* aCx,
                       const ArrayBufferViewOrArrayBuffer& aKey,
                       ErrorResult& aRv) const
{
  if (NS_FAILED(mUpdateError)) {
    aRv.Throw(mUpdateError);
    return MediaKeyStatus::Internal_error;
  }

  JS::Rooted<JSObject*> map(aCx, mMap);
  JS::Rooted<JS::Value> key(aCx);
  JS::Rooted<JS::Value> val(aCx);

  if (!aKey.ToJSVal(aCx, map, &key) ||
      !JS::MapGet(aCx, map, key, &val)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return MediaKeyStatus::Internal_error;
  }

  bool ok;
  int index = FindEnumStringIndex<true>(
      aCx, val, MediaKeyStatusValues::strings,
      "MediaKeyStatus", "Invalid MediaKeyStatus value", &ok);

  return ok ? static_cast<MediaKeyStatus>(index) :
              MediaKeyStatus::Internal_error;
}

bool
MediaKeyStatusMap::Has(JSContext* aCx,
                       const ArrayBufferViewOrArrayBuffer& aKey,
                       ErrorResult& aRv) const
{
  if (NS_FAILED(mUpdateError)) {
    aRv.Throw(mUpdateError);
    return false;
  }

  JS::Rooted<JSObject*> map(aCx, mMap);
  JS::Rooted<JS::Value> key(aCx);
  bool result = false;

  if (!aKey.ToJSVal(aCx, map, &key) ||
      !JS::MapHas(aCx, map, key, &result)) {
    aRv.Throw(NS_ERROR_FAILURE);
  }

  return result;
}

template<decltype(JS::MapKeys) Method>
static void CallMapMethod(JSContext* aCx,
                          const JS::Heap<JSObject*>& aMap,
                          JS::MutableHandle<JSObject*> aResult,
                          ErrorResult& aRv,
                          nsresult aUpdateError)
{
  if (NS_FAILED(aUpdateError)) {
    aRv.Throw(aUpdateError);
    return;
  }

  JS::Rooted<JSObject*> map(aCx, aMap);
  JS::Rooted<JS::Value> result(aCx);
  if (!Method(aCx, map, &result)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aResult.set(&result.toObject());
}

void
MediaKeyStatusMap::Keys(JSContext* aCx,
                        JS::MutableHandle<JSObject*> aResult,
                        ErrorResult& aRv) const
{
  CallMapMethod<JS::MapKeys>(aCx, mMap, aResult, aRv, mUpdateError);
}

void
MediaKeyStatusMap::Values(JSContext* aCx,
                          JS::MutableHandle<JSObject*> aResult,
                          ErrorResult& aRv) const
{
  CallMapMethod<JS::MapValues>(aCx, mMap, aResult, aRv, mUpdateError);
}
void
MediaKeyStatusMap::Entries(JSContext* aCx,
                           JS::MutableHandle<JSObject*> aResult,
                           ErrorResult& aRv) const
{
  CallMapMethod<JS::MapEntries>(aCx, mMap, aResult, aRv, mUpdateError);
}

uint32_t
MediaKeyStatusMap::GetSize(JSContext* aCx, ErrorResult& aRv) const
{
  if (NS_FAILED(mUpdateError)) {
    aRv.Throw(mUpdateError);
    return 0;
  }
  JS::Rooted<JSObject*> map(aCx, mMap);
  return JS::MapSize(aCx, map);
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

static bool
ToJSString(JSContext* aCx, GMPMediaKeyStatus aStatus,
           JS::MutableHandle<JS::Value> aResult)
{
  auto val = uint32_t(ToMediaKeyStatus(aStatus));
  MOZ_ASSERT(val < ArrayLength(MediaKeyStatusValues::strings));
  JSString* str = JS_NewStringCopyN(aCx,
      MediaKeyStatusValues::strings[val].value,
      MediaKeyStatusValues::strings[val].length);
  if (!str) {
    return false;
  }
  aResult.setString(str);
  return true;
}

nsresult
MediaKeyStatusMap::UpdateInternal(const nsTArray<CDMCaps::KeyStatus>& keys)
{
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mParent))) {
    return NS_ERROR_FAILURE;
  }

  jsapi.TakeOwnershipOfErrorReporting();
  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> map(cx, mMap);
  if (!JS::MapClear(cx, map)) {
    return NS_ERROR_FAILURE;
  }

  for (size_t i = 0; i < keys.Length(); i++) {
    const auto& ks = keys[i];
    JS::Rooted<JS::Value> key(cx);
    JS::Rooted<JS::Value> val(cx);
    if (!ToJSValue(cx, TypedArrayCreator<ArrayBuffer>(ks.mId), &key) ||
        !ToJSString(cx, ks.mStatus, &val) ||
        !JS::MapSet(cx, map, key, val)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

void
MediaKeyStatusMap::Update(const nsTArray<CDMCaps::KeyStatus>& keys)
{
  // Since we can't leave the map in a partial update state, we need
  // to remember the error and throw it next time the interface methods
  // are called.
  mUpdateError = UpdateInternal(keys);
}

} // namespace dom
} // namespace mozilla
