/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_FormControlAccessible_H_
#define MOZILLA_A11Y_FormControlAccessible_H_

#include "BaseAccessibles.h"

namespace mozilla {
namespace a11y {

/**
  * Generic class used for progress meters.
  */
template<int Max>
class ProgressMeterAccessible : public LeafAccessible
{
public:
  ProgressMeterAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    LeafAccessible(aContent, aDoc)
  {
    // Ignore 'ValueChange' DOM event in lieu of @value attribute change
    // notifications.
    mStateFlags |= eHasNumericValue | eIgnoreDOMUIEvent;
    mType = eProgressType;
  }

  // Accessible
  virtual void Value(nsString& aValue);
  virtual mozilla::a11y::role NativeRole() MOZ_OVERRIDE;
  virtual uint64_t NativeState() MOZ_OVERRIDE;

  // Value
  virtual double MaxValue() const MOZ_OVERRIDE;
  virtual double MinValue() const MOZ_OVERRIDE;
  virtual double CurValue() const MOZ_OVERRIDE;
  virtual double Step() const MOZ_OVERRIDE;
  virtual bool SetCurValue(double aValue) MOZ_OVERRIDE;

  // Widgets
  virtual bool IsWidget() const;

protected:
  virtual ~ProgressMeterAccessible() {}
};

/**
  * Generic class used for radio buttons.
  */
class RadioButtonAccessible : public LeafAccessible
{

public:
  RadioButtonAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole();

  // ActionAccessible
  virtual uint8_t ActionCount() MOZ_OVERRIDE;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) MOZ_OVERRIDE;
  virtual bool DoAction(uint8_t aIndex) MOZ_OVERRIDE;

  enum { eAction_Click = 0 };

  // Widgets
  virtual bool IsWidget() const;
};

} // namespace a11y
} // namespace mozilla

#endif

