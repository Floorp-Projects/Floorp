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
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;  // Add to make it buildable.

namespace mozilla {
namespace dom {

enum class VideoColorPrimaries : uint8_t;           // Add to make it buildable.
enum class VideoTransferCharacteristics : uint8_t;  // Add to make it buildable.
enum class VideoMatrixCoefficients : uint8_t;       // Add to make it buildable.
struct VideoColorSpaceInit;                         // Add to make it buildable.

}  // namespace dom
}  // namespace mozilla

namespace mozilla::dom {

class VideoColorSpace final
    : public nsISupports /* or NonRefcountedDOMObject if this is a
                            non-refcounted object */
    ,
      public nsWrapperCache /* Change wrapperCache in the binding configuration
                               if you don't want this */
{
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(VideoColorSpace)

 public:
  VideoColorSpace();

 protected:
  ~VideoColorSpace();

 public:
  // This should return something that eventually allows finding a
  // path to the global this object is associated with.  Most simply,
  // returning an actual global works.
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<VideoColorSpace> Constructor(
      const GlobalObject& global, const VideoColorSpaceInit& init,
      ErrorResult& aRv);

  Nullable<VideoColorPrimaries> GetPrimaries() const;

  Nullable<VideoTransferCharacteristics> GetTransfer() const;

  Nullable<VideoMatrixCoefficients> GetMatrix() const;

  Nullable<bool> GetFullRange() const;

  void ToJSON(JSContext* cx, JS::MutableHandle<JSObject*> aRetVal);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_VideoColorSpace_h
