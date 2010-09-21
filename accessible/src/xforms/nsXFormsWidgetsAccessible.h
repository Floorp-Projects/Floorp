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
  nsXFormsDropmarkerWidgetAccessible(nsIContent *aContent,
                                     nsIWeakReference *aShell);

  // nsIAccessible
  NS_IMETHOD GetNumActions(PRUint8 *aCount);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
};


/**
 * Accessible object for calendar widget. It is used by xforms:input[xsd:date].
 */
class nsXFormsCalendarWidgetAccessible : public nsAccessibleWrap
{
public:
  nsXFormsCalendarWidgetAccessible(nsIContent *aContent,
                                   nsIWeakReference *aShell);

  // nsAccessible
  virtual PRUint32 NativeRole();
};


/**
 * Accessible object for popup menu of minimal xforms select1 element that is
 * represented by combobox.
 */
class nsXFormsComboboxPopupWidgetAccessible : public nsXFormsAccessible
{
public:
  nsXFormsComboboxPopupWidgetAccessible(nsIContent *aContent,
                                        nsIWeakReference *aShell);

  // nsIAccessible
  NS_IMETHOD GetValue(nsAString& aValue);
  NS_IMETHOD GetDescription(nsAString& aDescription);

  // nsAccessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual PRUint32 NativeRole();
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);

protected:
  // nsAccessible
  virtual void CacheChildren();
};

#endif
