/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_FormControlAccessible_H_
#define MOZILLA_A11Y_FormControlAccessible_H_

#include "nsBaseWidgetAccessible.h"

namespace mozilla {
namespace a11y {

/**
  * Generic class used for progress meters.
  */
template<int Max>
class ProgressMeterAccessible : public nsLeafAccessible
{
public:
  ProgressMeterAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
    nsLeafAccessible(aContent, aDoc)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLEVALUE

  // nsAccessible
  virtual void Value(nsString& aValue);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // Widgets
  virtual bool IsWidget() const;
};

/**
  * Generic class used for radio buttons.
  */
class RadioButtonAccessible : public nsLeafAccessible
{

public:
  RadioButtonAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();

  // ActionAccessible
  virtual PRUint8 ActionCount();

  enum { eAction_Click = 0 };

  // Widgets
  virtual bool IsWidget() const;
};

} // namespace a11y
} // namespace mozilla

#endif

