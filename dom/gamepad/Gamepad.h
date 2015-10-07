/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_gamepad_Gamepad_h
#define mozilla_dom_gamepad_Gamepad_h

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/GamepadBinding.h"
#include "mozilla/dom/GamepadButton.h"
#include <stdint.h>
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"
#include "nsPerformance.h"

namespace mozilla {
namespace dom {

// Per spec:
// https://dvcs.w3.org/hg/gamepad/raw-file/default/gamepad.html#remapping
const int kStandardGamepadButtons = 17;
const int kStandardGamepadAxes = 4;

const int kButtonLeftTrigger = 6;
const int kButtonRightTrigger = 7;

const int kLeftStickXAxis = 0;
const int kLeftStickYAxis = 1;
const int kRightStickXAxis = 2;
const int kRightStickYAxis = 3;

class Gamepad final : public nsISupports,
                      public nsWrapperCache
{
public:
  Gamepad(nsISupports* aParent,
          const nsAString& aID, uint32_t aIndex,
          GamepadMappingType aMapping,
          uint32_t aNumButtons, uint32_t aNumAxes);
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Gamepad)

  void SetConnected(bool aConnected);
  void SetButton(uint32_t aButton, bool aPressed, double aValue);
  void SetAxis(uint32_t aAxis, double aValue);
  void SetIndex(uint32_t aIndex);

  // Make the state of this gamepad equivalent to other.
  void SyncState(Gamepad* aOther);

  // Return a new Gamepad containing the same data as this object,
  // parented to aParent.
  already_AddRefed<Gamepad> Clone(nsISupports* aParent);

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void GetId(nsAString& aID) const
  {
    aID = mID;
  }

  DOMHighResTimeStamp Timestamp() const
  {
     return mTimestamp;
  }

  GamepadMappingType Mapping()
  {
    return mMapping;
  }

  bool Connected() const
  {
    return mConnected;
  }

  uint32_t Index() const
  {
    return mIndex;
  }

  void GetButtons(nsTArray<nsRefPtr<GamepadButton>>& aButtons) const
  {
    aButtons = mButtons;
  }

  void GetAxes(nsTArray<double>& aAxes) const
  {
    aAxes = mAxes;
  }

private:
  virtual ~Gamepad() {}
  void UpdateTimestamp();

protected:
  nsCOMPtr<nsISupports> mParent;
  nsString mID;
  uint32_t mIndex;

  // The mapping in use.
  GamepadMappingType mMapping;

  // true if this gamepad is currently connected.
  bool mConnected;

  // Current state of buttons, axes.
  nsTArray<nsRefPtr<GamepadButton>> mButtons;
  nsTArray<double> mAxes;
  DOMHighResTimeStamp mTimestamp;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_gamepad_Gamepad_h
