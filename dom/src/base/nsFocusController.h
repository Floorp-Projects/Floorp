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

#ifndef nsFocusController_h__
#define nsFocusController_h__

#include "nsCOMPtr.h"
#include "nsIFocusController.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsWeakReference.h"

class nsIDOMElement;
class nsIDOMWindow;
class nsIController;
class nsIControllers;

class nsFocusController : public nsIFocusController, 
                          public nsIDOMFocusListener,
                          public nsSupportsWeakReference
{
public:
  static NS_IMETHODIMP Create(nsIFocusController** aResult);

protected:
  nsFocusController(void);
  virtual ~nsFocusController(void);

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetFocusedElement(nsIDOMElement** aResult);
  NS_IMETHOD SetFocusedElement(nsIDOMElement* aElement);

  NS_IMETHOD GetFocusedWindow(nsIDOMWindowInternal** aResult);
  NS_IMETHOD SetFocusedWindow(nsIDOMWindowInternal* aResult);

  NS_IMETHOD GetSuppressFocus(PRBool* aSuppressFlag);
  NS_IMETHOD SetSuppressFocus(PRBool aSuppressFlag, const char* aReason);

  NS_IMETHOD GetSuppressFocusScroll(PRBool* aSuppressFlag);
  NS_IMETHOD SetSuppressFocusScroll(PRBool aSuppressFlag);
  
  NS_IMETHOD GetActive(PRBool* aActive);
  NS_IMETHOD SetActive(PRBool aActive);

  NS_IMETHOD GetPopupNode(nsIDOMNode** aNode);
  NS_IMETHOD SetPopupNode(nsIDOMNode* aNode);

  NS_IMETHOD GetControllerForCommand(const nsAReadableString& aCommand, nsIController** aResult);
  NS_IMETHOD GetControllers(nsIControllers** aResult);

  NS_IMETHOD MoveFocus(PRBool aForward, nsIDOMElement* aElt);

  // nsIDOMFocusListener
  NS_IMETHOD Focus(nsIDOMEvent* aEvent);
  NS_IMETHOD Blur(nsIDOMEvent* aEvent);

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* anEvent) { return NS_OK; };

protected:
  NS_IMETHOD UpdateCommands(const nsAReadableString& aEventName);

public:
  static nsresult GetParentWindowFromDocument(nsIDOMDocument* aElement, nsIDOMWindowInternal** aWindow);

// Members
protected:
  nsCOMPtr<nsIDOMElement> mCurrentElement; // [OWNER]
  nsCOMPtr<nsIDOMWindowInternal> mCurrentWindow; // [OWNER]
  nsCOMPtr<nsIDOMNode> mPopupNode; // [OWNER]

  PRUint32 mSuppressFocus;
  PRBool mSuppressFocusScroll;
  PRBool mActive;
};

#endif // nsFocusController_h__
