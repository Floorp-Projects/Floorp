/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/VideoColorSpace.h"
#include "mozilla/dom/VideoColorSpaceBinding.h"

namespace mozilla::dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(VideoColorSpace)
NS_IMPL_CYCLE_COLLECTING_ADDREF(VideoColorSpace)
NS_IMPL_CYCLE_COLLECTING_RELEASE(VideoColorSpace)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(VideoColorSpace)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

VideoColorSpace::VideoColorSpace() {
  // Add |MOZ_COUNT_CTOR(VideoColorSpace);| for a non-refcounted object.
}

VideoColorSpace::~VideoColorSpace() {
  // Add |MOZ_COUNT_DTOR(VideoColorSpace);| for a non-refcounted object.
}

// Add to make it buildable.
nsIGlobalObject* VideoColorSpace::GetParentObject() const { return nullptr; }

JSObject* VideoColorSpace::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return VideoColorSpace_Binding::Wrap(aCx, this, aGivenProto);
}

// The followings are added to make it buildable.

/* static */
already_AddRefed<VideoColorSpace> VideoColorSpace::Constructor(
    const GlobalObject& global, const VideoColorSpaceInit& init,
    ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

Nullable<VideoColorPrimaries> VideoColorSpace::GetPrimaries() const {
  return nullptr;
}

Nullable<VideoTransferCharacteristics> VideoColorSpace::GetTransfer() const {
  return nullptr;
}

Nullable<VideoMatrixCoefficients> VideoColorSpace::GetMatrix() const {
  return nullptr;
}

Nullable<bool> VideoColorSpace::GetFullRange() const { return nullptr; }

void VideoColorSpace::ToJSON(JSContext* cx,
                             JS::MutableHandle<JSObject*> aRetVal) {}

}  // namespace mozilla::dom
