/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsXFormsFormControlsAccessible_H_
#define _nsXFormsFormControlsAccessible_H_

#include "nsXFormsAccessible.h"

/**
 * Accessible object for xforms:label.
 */

class nsXFormsLabelAccessible : public nsXFormsAccessible
{
public:
  nsXFormsLabelAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsIAccessible
  NS_IMETHOD GetDescription(nsAString& aDescription);

  // nsAccessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual PRUint32 NativeRole();
};

/**
 * Accessible object for xforms:output.
 */

class nsXFormsOutputAccessible : public nsXFormsAccessible
{
public:
  nsXFormsOutputAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsAccessible
  virtual PRUint32 NativeRole();
};

/**
 * Accessible object for xforms:trigger and xforms:submit.
 */

class nsXFormsTriggerAccessible : public nsXFormsAccessible
{
public:
  nsXFormsTriggerAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsIAccessible
  NS_IMETHOD GetValue(nsAString& aValue);

  NS_IMETHOD GetNumActions(PRUint8 *aCount);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsAccessible
  virtual PRUint32 NativeRole();
};

/**
 * Accessible object for xforms:input and xforms:textarea.
 */

class nsXFormsInputAccessible : public nsXFormsEditableAccessible
{
public:
  nsXFormsInputAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetNumActions(PRUint8 *aCount);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsAccessible
  virtual PRUint32 NativeRole();
};

/**
 * Accessible object for xforms:input[type="xsd:boolean"].
 */

class nsXFormsInputBooleanAccessible : public nsXFormsAccessible
{
public:
  nsXFormsInputBooleanAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsIAccessible
  NS_IMETHOD GetNumActions(PRUint8 *aCount);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
};

/**
 * Accessible object for xforms:input[type="xsd:date"].
 */

class nsXFormsInputDateAccessible : public nsXFormsContainerAccessible
{
public:
  nsXFormsInputDateAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsAccessible
  virtual PRUint32 NativeRole();
};

/**
 * Accessible object for xforms:secret.
 */

class nsXFormsSecretAccessible : public nsXFormsInputAccessible
{
public:
  nsXFormsSecretAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsIAccessible
  NS_IMETHOD GetValue(nsAString& aValue);

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
};


/**
 * Accessible object for xforms:range.
 */

class nsXFormsRangeAccessible : public nsXFormsAccessible
{
public:
  nsXFormsRangeAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsIAccessibleValue
  NS_IMETHOD GetMaximumValue(double *aMaximumValue);
  NS_IMETHOD GetMinimumValue(double *aMinimumValue);
  NS_IMETHOD GetMinimumIncrement(double *aMinimumIncrement);
  NS_IMETHOD GetCurrentValue(double *aCurrentValue);

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
};


/**
 * Accessible object for xforms:select and xforms:select1 that are implemented
 * using host document's native widget.
 */

class nsXFormsSelectAccessible : public nsXFormsContainerAccessible
{
public:
  nsXFormsSelectAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsAccessible
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
};


/**
 * Accessible object for xforms:choices.
 */

class nsXFormsChoicesAccessible : public nsXFormsAccessible
{
public:
  nsXFormsChoicesAccessible(nsIContent* aContent, nsIWeakReference *aShell);

  // nsIAccessible
  NS_IMETHOD GetValue(nsAString& aValue);

  // nsAccessible
  virtual PRUint32 NativeRole();

protected:
  // nsAccessible
  virtual void CacheChildren();
};


/**
 * Accessible object for xforms:select/xforms:select1 of full appearance that
 * may be represented by group of checkboxes or radiogroup.
 */

class nsXFormsSelectFullAccessible : public nsXFormsSelectableAccessible
{
public:
  nsXFormsSelectFullAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsAccessible
  virtual PRUint32 NativeRole();

protected:
  // nsAccessible
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
  nsXFormsItemCheckgroupAccessible(nsIContent *aContent,
                                   nsIWeakReference *aShell);

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
};


/**
 * Accessible object for a xforms:item when it is represented by a radiobutton.
 * This occurs when the item is contained in a xforms:select1 with full
 * appearance. Such a xforms:select1 is represented as a radiogroup.
 */

class nsXFormsItemRadiogroupAccessible : public nsXFormsSelectableItemAccessible
{
public:
  nsXFormsItemRadiogroupAccessible(nsIContent *aContent,
                                   nsIWeakReference *aShell);

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
};


/**
 * Accessible object for xforms:select1 of minimal appearance that is
 * represented by combobox.
 */

class nsXFormsSelectComboboxAccessible : public nsXFormsSelectableAccessible
{
public:
  nsXFormsSelectComboboxAccessible(nsIContent *aContent,
                                   nsIWeakReference *aShell);

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
  virtual PRBool GetAllowsAnonChildAccessibles();
};


/**
 * Accessible object for xforms:item element when it is represented by a
 * listitem. This occurs when the item is contained in a xforms:select with
 * minimal appearance. Such a xforms:select is represented by a combobox.
 */

class nsXFormsItemComboboxAccessible : public nsXFormsSelectableItemAccessible
{
public:
  nsXFormsItemComboboxAccessible(nsIContent *aContent,
                                 nsIWeakReference *aShell);

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
};

#endif

