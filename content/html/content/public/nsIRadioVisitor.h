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

#ifndef nsIRadioVisitor_h___
#define nsIRadioVisitor_h___

#include "nsISupports.h"
class nsIFormControl;

// IID for the nsIRadioControl interface
#define NS_IRADIOVISITOR_IID \
{ 0xd3494bd2, 0x1dd1, 0x11b2, \
  { 0xbe, 0x86, 0xb5, 0x08, 0xc8, 0x71, 0xd7, 0xc5 } }

/**
 * This interface is used for the text control frame to store its value away
 * into the content.
 */
class nsIRadioVisitor : public nsISupports {
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IRADIOVISITOR_IID)

  /**
   * Visit a node in the tree.  This is meant to be called on all radios in a
   * group, sequentially.  (Each radio group implementor may define
   * sequentially in their own way, it just has to be the same every time.)
   * Currently all radio groups are ordered in the order they appear in the
   * document.  Radio group implementors should honor the aStop parameter and
   * stop iterating over form controls when Visit() returns true there.
   *
   * @param aRadio the radio button in question (must be nsnull and QI'able to
   *               nsIRadioControlElement)
   * @param aStop whether or not to stop iterating (out-param)
   */
  NS_IMETHOD Visit(nsIFormControl* aRadio, PRBool* aStop) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIRadioVisitor, NS_IRADIOVISITOR_IID)

/**
 * This visitor sets CheckedChanged on all elements it finds.
 *
 * @param aVisitor the visitor (out param)
 * @param aCheckedChanged the value of CheckedChanged to set on all elements
 */
nsresult
NS_GetRadioSetCheckedChangedVisitor(PRBool aCheckedChanged,
                                    nsIRadioVisitor** aVisitor);

/**
 * This visitor will take the boolean you're pointing at and put
 * aCheckedChanged into it.  If the visitor is never called, aCheckedChanged
 * will of course not change.
 *
 * @param aVisitor the visitor (out param)
 * @param aCheckedChanged the boolean to put CheckedChanged into
 * @param aExcludeElement the element 
 */
nsresult
NS_GetRadioGetCheckedChangedVisitor(PRBool* aCheckedChanged,
                                    nsIFormControl* aExcludeElement,
                                    nsIRadioVisitor** aVisitor);

#endif // nsIRadioVisitor_h___
