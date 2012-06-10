/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULComboboxAccessible_h__
#define mozilla_a11y_XULComboboxAccessible_h__

#include "XULMenuAccessible.h"

namespace mozilla {
namespace a11y {

/**
 * Used for XUL comboboxes like xul:menulist and autocomplete textbox.
 */
class XULComboboxAccessible : public AccessibleWrap
{
public:
  enum { eAction_Click = 0 };

  XULComboboxAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD DoAction(PRUint8 aIndex);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);

  // Accessible
  virtual void Description(nsString& aDescription);
  virtual void Value(nsString& aValue);
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual bool CanHaveAnonChildren();

  // ActionAccessible
  virtual PRUint8 ActionCount();

  // Widgets
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;
};

} // namespace a11y
} // namespace mozilla

#endif
