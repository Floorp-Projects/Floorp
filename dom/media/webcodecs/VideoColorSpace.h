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

 private:
  template <class T>
  static Nullable<T> ToNullable(const Optional<T>& aInput) {
    if (aInput.WasPassed()) {
      return Nullable<T>(aInput.Value());
    }
    return Nullable<T>();
  }

 public:
  // This should return something that eventually allows finding a
  // path to the global this object is associated with.  Most simply,
  // returning an actual global works.
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<VideoColorSpace> Constructor(
      const GlobalObject& aGlobal, const VideoColorSpaceInit& aInit,
      ErrorResult& aRv);

  Nullable<VideoColorPrimaries> GetPrimaries() const {
    return ToNullable(mInit.mPrimaries);
  }

  Nullable<VideoTransferCharacteristics> GetTransfer() const {
    return ToNullable(mInit.mTransfer);
  }

  Nullable<VideoMatrixCoefficients> GetMatrix() const {
    return ToNullable(mInit.mMatrix);
  }

  Nullable<bool> GetFullRange() const { return ToNullable(mInit.mFullRange); }

 private:
  nsCOMPtr<nsIGlobalObject> mParent;
  const VideoColorSpaceInit mInit;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_VideoColorSpace_h
