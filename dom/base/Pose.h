/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Pose_h
#define mozilla_dom_Pose_h

#include "mozilla/ErrorResult.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class Pose : public nsWrapperCache {
 public:
  explicit Pose(nsISupports* aParent);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(Pose)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(Pose)

  nsISupports* GetParentObject() const;

  virtual void GetPosition(JSContext* aJSContext,
                           JS::MutableHandle<JSObject*> aRetval,
                           ErrorResult& aRv) = 0;
  virtual void GetLinearVelocity(JSContext* aJSContext,
                                 JS::MutableHandle<JSObject*> aRetval,
                                 ErrorResult& aRv) = 0;
  virtual void GetLinearAcceleration(JSContext* aJSContext,
                                     JS::MutableHandle<JSObject*> aRetval,
                                     ErrorResult& aRv) = 0;
  virtual void GetOrientation(JSContext* aJSContext,
                              JS::MutableHandle<JSObject*> aRetval,
                              ErrorResult& aRv) = 0;
  virtual void GetAngularVelocity(JSContext* aJSContext,
                                  JS::MutableHandle<JSObject*> aRetval,
                                  ErrorResult& aRv) = 0;
  virtual void GetAngularAcceleration(JSContext* aJSContext,
                                      JS::MutableHandle<JSObject*> aRetval,
                                      ErrorResult& aRv) = 0;
  static void SetFloat32Array(JSContext* aJSContext, nsWrapperCache* creator,
                              JS::MutableHandle<JSObject*> aRetVal,
                              JS::Heap<JSObject*>& aObj, float* aVal,
                              uint32_t aValLength, ErrorResult& aRv);

 protected:
  virtual ~Pose();

  nsCOMPtr<nsISupports> mParent;

  JS::Heap<JSObject*> mPosition;
  JS::Heap<JSObject*> mLinearVelocity;
  JS::Heap<JSObject*> mLinearAcceleration;
  JS::Heap<JSObject*> mOrientation;
  JS::Heap<JSObject*> mAngularVelocity;
  JS::Heap<JSObject*> mAngularAcceleration;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_Pose_h
