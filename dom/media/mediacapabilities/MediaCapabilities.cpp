/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaCapabilities.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/dom/MediaCapabilitiesBinding.h"
#include "mozilla/dom/Promise.h"

namespace mozilla {
namespace dom {

already_AddRefed<Promise>
MediaCapabilities::DecodingInfo(
  const MediaDecodingConfiguration& aConfiguration,
  ErrorResult& aRv)
{
  RefPtr<Promise> promise = Promise::Create(mParent, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  promise->MaybeReject(NS_ERROR_DOM_TYPE_ERR);

  return promise.forget();
}

already_AddRefed<Promise>
MediaCapabilities::EncodingInfo(
  const MediaEncodingConfiguration& aConfiguration,
  ErrorResult& aRv)
{
  RefPtr<Promise> promise = Promise::Create(mParent, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  promise->MaybeReject(NS_ERROR_DOM_TYPE_ERR);

  return promise.forget();
}

bool
MediaCapabilities::Enabled(JSContext* aCx, JSObject* aGlobal)
{
  return StaticPrefs::MediaCapabilitiesEnabled();
}

JSObject*
MediaCapabilities::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaCapabilities_Binding::Wrap(aCx, this, aGivenProto);
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaCapabilities)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaCapabilities)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaCapabilities)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaCapabilities, mParent)

// MediaCapabilitiesInfo
bool
MediaCapabilitiesInfo::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto,
                                  JS::MutableHandle<JSObject*> aReflector)
{
  return MediaCapabilitiesInfo_Binding::Wrap(
    aCx, this, aGivenProto, aReflector);
}

} // namespace dom
} // namespace mozilla
