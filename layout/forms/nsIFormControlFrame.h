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

#ifndef nsIFormControlFrame_h___
#define nsIFormControlFrame_h___

#include "nsISupports.h"
#include "nsFont.h"
class nsIPresContext;
class nsAString;
class nsIContent;
class nsIAtom;


// IID for the nsIFormControlFrame class
#define NS_IFORMCONTROLFRAME_IID    \
  { 0xf1911a34, 0xcdf7, 0x4f10, \
      { 0xbc, 0x2a, 0x77, 0x1f, 0x68, 0xce, 0xbc, 0x54 } }

/** 
  * nsIFormControlFrame is the common interface for frames of form controls. It
  * provides a uniform way of creating widgets, resizing, and painting.
  * @see nsLeafFrame and its base classes for more info
  */
class nsIFormControlFrame : public nsISupports {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFORMCONTROLFRAME_IID)

  NS_IMETHOD_(PRInt32) GetFormControlType() const =  0;

  NS_IMETHOD GetName(nsAString* aName) = 0;

  virtual void SetFocus(PRBool aOn = PR_TRUE, PRBool aRepaint = PR_FALSE) = 0;

  virtual void ScrollIntoView(nsIPresContext* aPresContext) = 0;  

  virtual void MouseClicked(nsIPresContext* aPresContext) = 0;

  /**
   * Set the suggested size for the form element. 
   * This is used to control the size of the element during reflow if it hasn't had it's size
   * explicitly set.
   * @param aWidth width of the form element
   * @param aHeight height of the form element
   * @returns NS_OK 
   */

  NS_IMETHOD SetSuggestedSize(nscoord aWidth, nscoord aHeight) = 0;
  
  /**
   * Get the content object associated with this frame. Adds a reference to
   * the content object so the caller must do a release.
   *
   * @see nsISupports#Release()
   */
  NS_IMETHOD GetFormContent(nsIContent*& aContent) const = 0;

  /**
   * Set a property on the form control frame.
   *
   * @param aName name of the property to set
   * @param aValue value of the property
   * @returns NS_OK if the property name is valid, otherwise an error code
   */
  
  NS_IMETHOD SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAString& aValue) = 0;
  
  /**
   * Get a property from the form control frame
   *
   * @param aName name of the property to get
   * @param aValue value of the property
   * @returns NS_OK if the property name is valid, otherwise an error code
   */

  NS_IMETHOD GetProperty(nsIAtom* aName, nsAString& aValue) = 0; 

  /**
   * Notification that the content has been reset
   */
  NS_IMETHOD OnContentReset() = 0;

};

#endif

