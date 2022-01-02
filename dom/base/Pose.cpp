/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/experimental/TypedData.h"  // JS_GetFloat32ArrayData
#include "mozilla/ErrorResult.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/Pose.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(Pose)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Pose)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mPosition = nullptr;
  tmp->mLinearVelocity = nullptr;
  tmp->mLinearAcceleration = nullptr;
  tmp->mOrientation = nullptr;
  tmp->mAngularVelocity = nullptr;
  tmp->mAngularAcceleration = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Pose)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Pose)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mPosition)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mLinearVelocity)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mLinearAcceleration)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mOrientation)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mAngularVelocity)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mAngularAcceleration)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(Pose, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(Pose, Release)

Pose::Pose(nsISupports* aParent)
    : mParent(aParent),
      mPosition(nullptr),
      mLinearVelocity(nullptr),
      mLinearAcceleration(nullptr),
      mOrientation(nullptr),
      mAngularVelocity(nullptr),
      mAngularAcceleration(nullptr) {
  mozilla::HoldJSObjects(this);
}

Pose::~Pose() { mozilla::DropJSObjects(this); }

nsISupports* Pose::GetParentObject() const { return mParent; }

void Pose::SetFloat32Array(JSContext* aJSContext, nsWrapperCache* creator,
                           JS::MutableHandle<JSObject*> aRetVal,
                           JS::Heap<JSObject*>& aObj, float* aVal,
                           uint32_t aValLength, ErrorResult& aRv) {
  if (!aVal) {
    aRetVal.set(nullptr);
    return;
  }

  if (!aObj) {
    aObj = Float32Array::Create(aJSContext, creator, aValLength, aVal);
    if (!aObj) {
      aRv.NoteJSContextException(aJSContext);
      return;
    }
  } else {
    JS::AutoCheckCannotGC nogc;
    bool isShared = false;
    JS::RootedObject obj(aJSContext, aObj.get());
    float* data = JS_GetFloat32ArrayData(obj, &isShared, nogc);
    if (data) {
      memcpy(data, aVal, aValLength * sizeof(float));
    }
  }

  aRetVal.set(aObj);
}

}  // namespace mozilla::dom
