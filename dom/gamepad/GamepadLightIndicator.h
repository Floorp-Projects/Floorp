/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_gamepad_GamepadLightIndicator_h
#define mozilla_dom_gamepad_GamepadLightIndicator_h

#include "mozilla/dom/GamepadLightIndicatorBinding.h"
#include "mozilla/dom/GamepadHandle.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class GamepadLightIndicator final : public nsISupports, public nsWrapperCache {
 public:
  GamepadLightIndicator(nsISupports* aParent, GamepadHandle aGamepadHandle,
                        uint32_t aIndex);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(GamepadLightIndicator)

  static GamepadLightIndicatorType DefaultType() {
    return GamepadLightIndicatorType::Rgb;
  }

  nsISupports* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise> SetColor(const GamepadLightColor& color,
                                     ErrorResult& aRv);

  void SetType(GamepadLightIndicatorType aType) { mType = aType; }

  GamepadLightIndicatorType Type() const;

  void Set(const GamepadLightIndicator* aOther);

 private:
  virtual ~GamepadLightIndicator();

  nsCOMPtr<nsISupports> mParent;
  GamepadLightIndicatorType mType;
  GamepadHandle mGamepadHandle;
  uint32_t mIndex;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_gamepad_GamepadLightIndicator_h
