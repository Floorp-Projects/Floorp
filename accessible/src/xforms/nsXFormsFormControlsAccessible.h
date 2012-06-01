/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsXFormsFormControlsAccessible_H_
#define _nsXFormsFormControlsAccessible_H_

#include "nsXFormsAccessible.h"

/**
 * Accessible object for xforms:label.
 */

class nsXFormsLabelAccessible : public nsXFormsAccessible
{
public:
  nsXFormsLabelAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual void Description(nsString& aDescription);
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual mozilla::a11y::role NativeRole();
};

/**
 * Accessible object for xforms:output.
 */

class nsXFormsOutputAccessible : public nsXFormsAccessible
{
public:
  nsXFormsOutputAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
};

/**
 * Accessible object for xforms:trigger and xforms:submit.
 */

class nsXFormsTriggerAccessible : public nsXFormsAccessible
{
public:
  nsXFormsTriggerAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // Accessible
  virtual void Value(nsString& aValue);
  virtual mozilla::a11y::role NativeRole();

  // ActionAccessible
  virtual PRUint8 ActionCount();
};

/**
 * Accessible object for xforms:input and xforms:textarea.
 */

class nsXFormsInputAccessible : public nsXFormsEditableAccessible
{
public:
  nsXFormsInputAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // Accessible
  virtual mozilla::a11y::role NativeRole();

  // ActionAccessible
  virtual PRUint8 ActionCount();
};

/**
 * Accessible object for xforms:input[type="xsd:boolean"].
 */

class nsXFormsInputBooleanAccessible : public nsXFormsAccessible
{
public:
  nsXFormsInputBooleanAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // ActionAccessible
  virtual PRUint8 ActionCount();
};

/**
 * Accessible object for xforms:input[type="xsd:date"].
 */

class nsXFormsInputDateAccessible : public nsXFormsContainerAccessible
{
public:
  nsXFormsInputDateAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
};

/**
 * Accessible object for xforms:secret.
 */

class nsXFormsSecretAccessible : public nsXFormsInputAccessible
{
public:
  nsXFormsSecretAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual void Value(nsString& aValue);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();
};


/**
 * Accessible object for xforms:range.
 */

class nsXFormsRangeAccessible : public nsXFormsAccessible
{
public:
  nsXFormsRangeAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsIAccessibleValue
  NS_IMETHOD GetMaximumValue(double *aMaximumValue);
  NS_IMETHOD GetMinimumValue(double *aMinimumValue);
  NS_IMETHOD GetMinimumIncrement(double *aMinimumIncrement);
  NS_IMETHOD GetCurrentValue(double *aCurrentValue);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();
};


/**
 * Accessible object for xforms:select and xforms:select1 that are implemented
 * using host document's native widget.
 */

class nsXFormsSelectAccessible : public nsXFormsContainerAccessible
{
public:
  nsXFormsSelectAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual PRUint64 NativeState();
};


/**
 * Accessible object for xforms:choices.
 */

class nsXFormsChoicesAccessible : public nsXFormsAccessible
{
public:
  nsXFormsChoicesAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsIAccessible

  // Accessible
  virtual void Value(nsString& aValue);
  virtual mozilla::a11y::role NativeRole();

protected:
  // Accessible
  virtual void CacheChildren();
};


/**
 * Accessible object for xforms:select/xforms:select1 of full appearance that
 * may be represented by group of checkboxes or radiogroup.
 */

class nsXFormsSelectFullAccessible : public nsXFormsSelectableAccessible
{
public:
  nsXFormsSelectFullAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole();

protected:
  // Accessible
  virtual void CacheChildren();
};


/**
 * Accessible object for a xforms:item when it is represented by a checkbox.
 * This occurs when the item is contained in a xforms:select with full
 * appearance. Such a xforms:select is represented by a checkgroup.
 */

class nsXFormsItemCheckgroupAccessible : public nsXFormsSelectableItemAccessible
{
public:
  nsXFormsItemCheckgroupAccessible(nsIContent* aContent,
                                   DocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();
};


/**
 * Accessible object for a xforms:item when it is represented by a radiobutton.
 * This occurs when the item is contained in a xforms:select1 with full
 * appearance. Such a xforms:select1 is represented as a radiogroup.
 */

class nsXFormsItemRadiogroupAccessible : public nsXFormsSelectableItemAccessible
{
public:
  nsXFormsItemRadiogroupAccessible(nsIContent* aContent,
                                   DocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();
};


/**
 * Accessible object for xforms:select1 of minimal appearance that is
 * represented by combobox.
 */

class nsXFormsSelectComboboxAccessible : public nsXFormsSelectableAccessible
{
public:
  nsXFormsSelectComboboxAccessible(nsIContent* aContent,
                                   DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual bool CanHaveAnonChildren();
};


/**
 * Accessible object for xforms:item element when it is represented by a
 * listitem. This occurs when the item is contained in a xforms:select with
 * minimal appearance. Such a xforms:select is represented by a combobox.
 */

class nsXFormsItemComboboxAccessible : public nsXFormsSelectableItemAccessible
{
public:
  nsXFormsItemComboboxAccessible(nsIContent* aContent,
                                 DocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();
};

#endif

