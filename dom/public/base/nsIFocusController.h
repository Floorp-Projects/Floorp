/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   David W. Hyatt <hyatt@netscape.com> (Original Author)
 *   Dan Rosen <dr@netscape.com>
 */

#ifndef nsIFocusController_h__
#define nsIFocusController_h__

#include "nsISupports.h"

class nsIDOMElement;
class nsIDOMNode;
class nsIDOMWindowInternal;
class nsIController;
class nsIControllers;

// {AC71F479-17E1-4ee0-8EFD-0ECF2AA2F827}
#define NS_IFOCUSCONTROLLER_IID \
{ 0xac71f479, 0x17e1, 0x4ee0, { 0x8e, 0xfd, 0xe, 0xcf, 0x2a, 0xa2, 0xf8, 0x27 } }

class nsIFocusController : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IFOCUSCONTROLLER_IID; return iid; }

  NS_IMETHOD GetFocusedElement(nsIDOMElement** aResult)=0;
  NS_IMETHOD SetFocusedElement(nsIDOMElement* aElement)=0;

  NS_IMETHOD GetFocusedWindow(nsIDOMWindowInternal** aResult)=0;
  NS_IMETHOD SetFocusedWindow(nsIDOMWindowInternal* aResult)=0;

  NS_IMETHOD GetSuppressFocus(PRBool* aSuppressFlag)=0;
  NS_IMETHOD SetSuppressFocus(PRBool aSuppressFlag, const char* aReason)=0;

  NS_IMETHOD GetSuppressFocusScroll(PRBool* aSuppressFlag)=0;
  NS_IMETHOD SetSuppressFocusScroll(PRBool aSuppressFlag)=0;
  
  NS_IMETHOD GetActive(PRBool* aActive)=0;
  NS_IMETHOD SetActive(PRBool aActive)=0;

  NS_IMETHOD GetPopupNode(nsIDOMNode** aNode)=0;
  NS_IMETHOD SetPopupNode(nsIDOMNode* aNode)=0;

  NS_IMETHOD GetControllerForCommand(const nsAReadableString& aCommand, nsIController** aResult)=0;
  NS_IMETHOD GetControllers(nsIControllers** aResult)=0;

  NS_IMETHOD MoveFocus(PRBool aForward, nsIDOMElement* aElt)=0;
};

#endif // nsIFocusController_h__
