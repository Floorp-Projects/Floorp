/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GamepadTouch.h"
#include "mozilla/dom/GamepadManager.h"
#include "mozilla/dom/Promise.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(GamepadTouch)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(GamepadTouch)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mPosition = nullptr;
  tmp->mSurfaceDimensions = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(GamepadTouch)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(GamepadTouch)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mPosition)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mSurfaceDimensions)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(GamepadTouch, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(GamepadTouch, Release)

GamepadTouch::GamepadTouch(nsISupports* aParent)
    : mParent(aParent), mPosition(nullptr), mSurfaceDimensions(nullptr) {
  mozilla::HoldJSObjects(this);
  mTouchState = GamepadTouchState();
}

GamepadTouch::~GamepadTouch() { mozilla::DropJSObjects(this); }

/* virtual */ JSObject* GamepadTouch::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return GamepadTouch_Binding::Wrap(aCx, this, aGivenProto);
}

void GamepadTouch::GetPosition(JSContext* aCx,
                               JS::MutableHandle<JSObject*> aRetval,
                               ErrorResult& aRv) {
  mPosition = Float32Array::Create(aCx, this, 2, mTouchState.position);
  if (!mPosition) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  aRetval.set(mPosition);
}

void GamepadTouch::GetSurfaceDimensions(JSContext* aCx,
                                        JS::MutableHandle<JSObject*> aRetval,
                                        ErrorResult& aRv) {
  mSurfaceDimensions = Uint32Array::Create(aCx, this, 2,
                                           mTouchState.isSurfaceDimensionsValid
                                               ? mTouchState.surfaceDimensions
                                               : nullptr);

  if (!mSurfaceDimensions) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  aRetval.set(mSurfaceDimensions);
}

void GamepadTouch::SetTouchState(const GamepadTouchState& aTouch) {
  mTouchState = aTouch;
}

void GamepadTouch::Set(const GamepadTouch* aOther) {
  MOZ_ASSERT(aOther);
  mTouchState = aOther->mTouchState;
}

}  // namespace dom
}  // namespace mozilla
