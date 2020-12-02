/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GamepadLightIndicator.h"
#include "mozilla/dom/GamepadManager.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/HoldDropJSObjects.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(GamepadLightIndicator)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GamepadLightIndicator)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GamepadLightIndicator)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(GamepadLightIndicator, mParent)

GamepadLightIndicator::GamepadLightIndicator(nsISupports* aParent,
                                             GamepadHandle aGamepadHandle,
                                             uint32_t aIndex)
    : mParent(aParent),
      mType(DefaultType()),
      mGamepadHandle(aGamepadHandle),
      mIndex(aIndex) {}

GamepadLightIndicator::~GamepadLightIndicator() {
  mozilla::DropJSObjects(this);
}

/* virtual */ JSObject* GamepadLightIndicator::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return GamepadLightIndicator_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports* GamepadLightIndicator::GetParentObject() const { return mParent; }

already_AddRefed<Promise> GamepadLightIndicator::SetColor(
    const GamepadLightColor& color, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  MOZ_ASSERT(global);

  RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
  MOZ_ASSERT(gamepadManager);

  RefPtr<Promise> promise = gamepadManager->SetLightIndicatorColor(
      mGamepadHandle, mIndex, color.mRed, color.mGreen, color.mBlue, global,
      aRv);
  if (!promise) {
    return nullptr;
  }
  return promise.forget();
}

GamepadLightIndicatorType GamepadLightIndicator::Type() const { return mType; }

void GamepadLightIndicator::Set(const GamepadLightIndicator* aOther) {
  MOZ_ASSERT(aOther);
  mGamepadHandle = aOther->mGamepadHandle;
  mType = aOther->mType;
  mIndex = aOther->mIndex;
}

}  // namespace mozilla::dom
