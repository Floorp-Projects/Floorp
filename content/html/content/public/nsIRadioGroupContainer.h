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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#ifndef nsIRadioGroupContainer_h___
#define nsIRadioGroupContainer_h___

#include "nsISupports.h"

class nsIDOMHTMLInputElement;
class nsString;
class nsIRadioVisitor;
class nsIFormControl;

#define NS_IRADIOGROUPCONTAINER_IID   \
{ 0x06de7839, 0xd0db, 0x47d3, \
  { 0x82, 0x90, 0x3c, 0xb8, 0x62, 0x2e, 0xd9, 0x66 } }

/**
 * A container that has multiple radio groups in it, defined by name.
 */
class nsIRadioGroupContainer : public nsISupports
{
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRADIOGROUPCONTAINER_IID)

  /**
   * Walk through the radio group, visiting each note with avisitor->Visit()
   * @param aName the group name
   * @param aVisitor the visitor to visit with
   */
  NS_IMETHOD WalkRadioGroup(const nsAString& aName,
                            nsIRadioVisitor* aVisitor) = 0;

  /**
   * Set the current radio button in a group
   * @param aName the group name
   * @param aRadio the currently selected radio button
   */
  NS_IMETHOD SetCurrentRadioButton(const nsAString& aName,
                                   nsIDOMHTMLInputElement* aRadio) = 0;

  /**
   * Get the current radio button in a group
   * @param aName the group name
   * @param aRadio the currently selected radio button [OUT]
   */
  NS_IMETHOD GetCurrentRadioButton(const nsAString& aName,
                                   nsIDOMHTMLInputElement** aRadio) = 0;

  /**
   * Set the next/prev radio button in a group
   * @param aName the group name
   * @param aPrevious, true gets previous radio button, false gets next
   * @param aFocusedRadio the currently focused radio button
   * @param aRadio the currently selected radio button [OUT]
   */
  NS_IMETHOD GetNextRadioButton(const nsAString& aName,
                                const PRBool aPrevious,
                                nsIDOMHTMLInputElement*  aFocusedRadio,
                                nsIDOMHTMLInputElement** aRadio) = 0;

  /**
  /**
   * Add radio button to radio group
   *
   * Note that forms do not do anything for this method since they already
   * store radio groups on their own.
   *
   * @param aName radio group's name
   * @param aRadio radio button's pointer
   */
  NS_IMETHOD AddToRadioGroup(const nsAString& aName,
                             nsIFormControl* aRadio) = 0;

  /**
   * Remove radio button from radio group
   *
   * Note that forms do not do anything for this method since they already
   * store radio groups on their own.
   *
   * @param aName radio group's name
   * @param aRadio radio button's pointer
   */
  NS_IMETHOD RemoveFromRadioGroup(const nsAString& aName,
                                  nsIFormControl* aRadio) = 0;
};

#endif /* nsIRadioGroupContainer_h__ */
