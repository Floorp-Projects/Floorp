/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GamepadTouch.h"

#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/GamepadManager.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TypedArray.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WITH_JS_MEMBERS(GamepadTouch, (mParent),
                                                      (mPosition,
                                                       mSurfaceDimensions))

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
  mPosition = Float32Array::Create(aCx, this, mTouchState.position, aRv);
  if (aRv.Failed()) {
    return;
  }

  aRetval.set(mPosition);
}

void GamepadTouch::GetSurfaceDimensions(JSContext* aCx,
                                        JS::MutableHandle<JSObject*> aRetval,
                                        ErrorResult& aRv) {
  if (mTouchState.isSurfaceDimensionsValid) {
    mSurfaceDimensions =
        Uint32Array::Create(aCx, this, mTouchState.surfaceDimensions, aRv);
  } else {
    mSurfaceDimensions = Uint32Array::Create(
        aCx, this, std::size(mTouchState.surfaceDimensions), aRv);
  }

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

}  // namespace mozilla::dom
