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
 * John B. Keiser
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsIRadioControlElement_h___
#define nsIRadioControlElement_h___

#include "nsISupports.h"
#include "nsIRadioGroupContainer.h"
class nsAString;
class nsIForm;

// IID for the nsIRadioControl interface
#define NS_IRADIOCONTROLELEMENT_IID \
{ 0x647dff84, 0x1dd2, 0x11b2, \
    { 0x95, 0xcf, 0xe5, 0x01, 0xa8, 0x54, 0xa6, 0x40 } }

/**
 * This interface is used for the text control frame to store its value away
 * into the content.
 */
class nsIRadioControlElement : public nsISupports {
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IRADIOCONTROLELEMENT_IID)

  /**
   * Set the radio button checked, but don't do any extra checks and don't
   * trigger "value changed".  This is for experts only.  Do not try this
   * at home.  Well, OK, try it at home, but not late at night.
   */
  NS_IMETHOD RadioSetChecked(PRBool aNotify) = 0;

  /**
   * Let this radio button know that the radio group's "checked" property has
   * been changed by JavaScript or by the user
   *
   * @param aCheckedChanged whether the checked property has been changed.
   * @param aOnlyOnThisRadioButton set to TRUE to only set the property only on
   *        the radio button in question; otherwise it will set the property on
   *        all other radio buttons in its group just to be sure they are
   *        consistent.
   */
  NS_IMETHOD SetCheckedChangedInternal(PRBool aCheckedChanged) = 0;

  /**
   * Let an entire radio group know that its "checked" property has been
   * changed by JS or by the user (calls SetCheckedChangedInternal multiple
   * times via a visitor)
   *
   * @param aCheckedChanged whether the checked property has been changed.
   */
  NS_IMETHOD SetCheckedChanged(PRBool aCheckedChanged) = 0;

  /**
   * Find out whether this radio group's "checked" property has been changed by
   * JavaScript or by the user
   *
   * @param aCheckedChanged out param, whether the checked property has been
   *        changed.
   */
  NS_IMETHOD GetCheckedChanged(PRBool* aCheckedChanged) = 0;

  /**
   * Let this radio button know that state has been changed such that it has
   * been added to a group.
   */
  NS_IMETHOD AddedToRadioGroup(PRBool aNotify = PR_TRUE) = 0;

  /**
   * Let this radio button know that it is about to be removed from the radio
   * group it is currently in (the relevant properties should not have changed
   * yet).
   */
  NS_IMETHOD WillRemoveFromRadioGroup() = 0;

  /**
   * Get the radio group container for this radio button
   * @return the radio group container (or null if no container)
   */
  virtual already_AddRefed<nsIRadioGroupContainer> GetRadioGroupContainer() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIRadioControlElement,
                              NS_IRADIOCONTROLELEMENT_IID)

#endif // nsIRadioControlElement_h___
