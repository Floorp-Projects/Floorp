/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=3 sw=2 et tw=80:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David W. Hyatt <hyatt@netscape.com> (Original Author)
 *   Dan Rosen <dr@netscape.com>
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

#ifndef nsIFocusController_h__
#define nsIFocusController_h__

#include "nsISupports.h"
#include "nsCOMPtr.h"

class nsIDOMElement;
class nsIDOMNode;
class nsIDOMWindowInternal;
class nsIController;
class nsIControllers;
class nsAString;

// {da47ea2a-5e9a-4281-9b5b-a08c0e6b1fa5}
#define NS_IFOCUSCONTROLLER_IID \
{ 0xda47ea2a, 0x5e9a, 0x4281, { 0x9b, 0x5b, 0xa0, 0x8c, 0x0e, 0x6b, 0x1f, 0xa5 } }

class nsIFocusController : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFOCUSCONTROLLER_IID)

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

  NS_IMETHOD GetPopupEvent(nsIDOMEvent** aEvent)=0;
  NS_IMETHOD SetPopupEvent(nsIDOMEvent* aEvent)=0;

  NS_IMETHOD GetControllerForCommand(const char * aCommand, nsIController** aResult)=0;
  NS_IMETHOD GetControllers(nsIControllers** aResult)=0;

  NS_IMETHOD MoveFocus(PRBool aForward, nsIDOMElement* aElt)=0;

  NS_IMETHOD ResetElementFocus() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFocusController, NS_IFOCUSCONTROLLER_IID)

class nsFocusSuppressor {
public:
  ~nsFocusSuppressor()
  {
    Unsuppress();
  }

  // Note: We assume that aReason outlives the instance of this class.
  void Suppress(nsIFocusController *aController, const char *aReason)
  {
    Unsuppress();

    mController = aController;
    mReason = aReason;
    if (aController) {
      mController->SetSuppressFocus(PR_TRUE, mReason);
    }
  }

  void Unsuppress()
  {
    if (mController) {
      mController->SetSuppressFocus(PR_FALSE, mReason);
      mController = nsnull;
      mReason = nsnull;
    }
  }

  PRBool Suppressing()
  {
    return mController != nsnull;
  }

private:
  nsCOMPtr<nsIFocusController> mController;
  const char *mReason;
};

#endif // nsIFocusController_h__
