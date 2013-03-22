/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDomGamepad_h
#define nsDomGamepad_h

#include "mozilla/StandardInteger.h"
#include "nsIDOMGamepad.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

class nsDOMGamepad : public nsIDOMGamepad
{
public:
  nsDOMGamepad(const nsAString& aID, uint32_t aIndex,
               uint32_t aNumButtons, uint32_t aNumAxes);
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMGAMEPAD

  nsDOMGamepad();
  void SetConnected(bool aConnected);
  void SetButton(uint32_t aButton, double aValue);
  void SetAxis(uint32_t aAxis, double aValue);
  void SetIndex(uint32_t aIndex);

  // Make the state of this gamepad equivalent to other.
  void SyncState(nsDOMGamepad* other);

  // Return a new nsDOMGamepad containing the same data as this object.
  already_AddRefed<nsDOMGamepad> Clone();

private:
  virtual ~nsDOMGamepad() {}

protected:
  nsString mID;
  uint32_t mIndex;

  // true if this gamepad is currently connected.
  bool mConnected;

  // Current state of buttons, axes.
  nsTArray<double> mButtons;
  nsTArray<double> mAxes;
};

#endif // nsDomGamepad_h
