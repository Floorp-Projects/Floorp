/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/VideoColorSpace.h"
#include "mozilla/dom/VideoColorSpaceBinding.h"
#include "nsIGlobalObject.h"

namespace mozilla::dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(VideoColorSpace, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(VideoColorSpace)
NS_IMPL_CYCLE_COLLECTING_RELEASE(VideoColorSpace)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(VideoColorSpace)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

VideoColorSpace::VideoColorSpace(nsIGlobalObject* aParent,
                                 const VideoColorSpaceInit& aInit)
    : mParent(aParent), mInit(aInit) {
  MOZ_ASSERT(mParent);
}

nsIGlobalObject* VideoColorSpace::GetParentObject() const { return mParent; }

JSObject* VideoColorSpace::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return VideoColorSpace_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<VideoColorSpace> VideoColorSpace::Constructor(
    const GlobalObject& aGlobal, const VideoColorSpaceInit& aInit,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  RefPtr<VideoColorSpace> videoColorSpace(new VideoColorSpace(global, aInit));
  return aRv.Failed() ? nullptr : videoColorSpace.forget();
}

}  // namespace mozilla::dom
