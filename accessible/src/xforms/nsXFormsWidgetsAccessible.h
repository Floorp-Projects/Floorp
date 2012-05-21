/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsXFormsWidgetsAccessible_H_
#define _nsXFormsWidgetsAccessible_H_

#include "nsXFormsAccessible.h"
#include "nsBaseWidgetAccessible.h"

/**
 * Accessible object for dropmarker widget that is used inside xforms elements
 * of combobox representation. For example, these are xforms:select1,
 * xforms:input[type="xsd:date"].
 */
class nsXFormsDropmarkerWidgetAccessible : public nsLeafAccessible,
                                           public nsXFormsAccessibleBase
{
public:
  nsXFormsDropmarkerWidgetAccessible(nsIContent* aContent,
                                     nsDocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // ActionAccessible
  virtual PRUint8 ActionCount();
};


/**
 * Accessible object for calendar widget. It is used by xforms:input[xsd:date].
 */
class nsXFormsCalendarWidgetAccessible : public nsAccessibleWrap
{
public:
  nsXFormsCalendarWidgetAccessible(nsIContent* aContent,
                                   nsDocAccessible* aDoc);

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
};


/**
 * Accessible object for popup menu of minimal xforms select1 element that is
 * represented by combobox.
 */
class nsXFormsComboboxPopupWidgetAccessible : public nsXFormsAccessible
{
public:
  nsXFormsComboboxPopupWidgetAccessible(nsIContent* aContent,
                                        nsDocAccessible* aDoc);

  // nsAccessible
  virtual void Description(nsString& aDescription);
  virtual void Value(nsString& aValue);
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

protected:
  // nsAccessible
  virtual void CacheChildren();
};

#endif
