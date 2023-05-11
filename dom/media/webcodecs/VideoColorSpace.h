/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VideoColorSpace_h
#define mozilla_dom_VideoColorSpace_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/VideoColorSpaceBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla::dom {

class VideoColorSpace final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(VideoColorSpace)

 public:
  VideoColorSpace(nsIGlobalObject* aParent, const VideoColorSpaceInit& aInit);

 protected:
  ~VideoColorSpace() = default;

 public:
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<VideoColorSpace> Constructor(
      const GlobalObject& aGlobal, const VideoColorSpaceInit& aInit,
      ErrorResult& aRv);

  const Nullable<VideoColorPrimaries>& GetPrimaries() const {
    return mInit.mPrimaries;
  }

  const Nullable<VideoTransferCharacteristics>& GetTransfer() const {
    return mInit.mTransfer;
  }

  const Nullable<VideoMatrixCoefficients>& GetMatrix() const {
    return mInit.mMatrix;
  }

  const Nullable<bool>& GetFullRange() const { return mInit.mFullRange; }

 private:
  nsCOMPtr<nsIGlobalObject> mParent;
  const VideoColorSpaceInit mInit;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_VideoColorSpace_h
