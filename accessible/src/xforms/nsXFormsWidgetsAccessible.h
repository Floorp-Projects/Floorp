/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsXFormsWidgetsAccessible_H_
#define _nsXFormsWidgetsAccessible_H_

#include "BaseAccessibles.h"
#include "nsXFormsAccessible.h"

/**
 * Accessible object for dropmarker widget that is used inside xforms elements
 * of combobox representation. For example, these are xforms:select1,
 * xforms:input[type="xsd:date"].
 */
class nsXFormsDropmarkerWidgetAccessible : public mozilla::a11y::LeafAccessible,
                                           public nsXFormsAccessibleBase
{
public:
  nsXFormsDropmarkerWidgetAccessible(nsIContent* aContent,
                                     DocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD GetActionName(uint8_t aIndex, nsAString& aName);
  NS_IMETHOD DoAction(uint8_t aIndex);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual uint64_t NativeState();

  // ActionAccessible
  virtual uint8_t ActionCount();
};


/**
 * Accessible object for calendar widget. It is used by xforms:input[xsd:date].
 */
class nsXFormsCalendarWidgetAccessible : public AccessibleWrap
{
public:
  nsXFormsCalendarWidgetAccessible(nsIContent* aContent,
                                   DocAccessible* aDoc);

  // Accessible
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
                                        DocAccessible* aDoc);

  // Accessible
  virtual void Description(nsString& aDescription);
  virtual void Value(nsString& aValue);
  virtual mozilla::a11y::role NativeRole();
  virtual uint64_t NativeState();
  virtual uint64_t NativeInteractiveState() const;

protected:
  // Accessible
  virtual mozilla::a11y::ENameValueFlag NativeName(nsString& aName) MOZ_OVERRIDE;
  virtual void CacheChildren();
};

#endif
