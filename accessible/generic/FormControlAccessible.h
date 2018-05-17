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
  virtual void Value(nsString& aValue) const override;
  virtual mozilla::a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;

  // Value
  virtual double MaxValue() const override;
  virtual double MinValue() const override;
  virtual double CurValue() const override;
  virtual double Step() const override;
  virtual bool SetCurValue(double aValue) override;

  // Widgets
  virtual bool IsWidget() const override;

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
  virtual mozilla::a11y::role NativeRole() const override;

  // ActionAccessible
  virtual uint8_t ActionCount() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) const override;

  enum { eAction_Click = 0 };

  // Widgets
  virtual bool IsWidget() const override;
};

} // namespace a11y
} // namespace mozilla

#endif

