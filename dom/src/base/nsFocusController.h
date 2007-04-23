/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsFocusController_h__
#define nsFocusController_h__

#include "nsCOMPtr.h"
#include "nsIFocusController.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsWeakReference.h"
#include "nsCycleCollectionParticipant.h"

class nsIDOMElement;
class nsIDOMWindow;
class nsPIDOMWindow;
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
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

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

  NS_IMETHOD GetPopupEvent(nsIDOMEvent** aEvent);
  NS_IMETHOD SetPopupEvent(nsIDOMEvent* aEvent);

  NS_IMETHOD GetControllerForCommand(const char *aCommand, nsIController** aResult);
  NS_IMETHOD GetControllers(nsIControllers** aResult);

  NS_IMETHOD MoveFocus(PRBool aForward, nsIDOMElement* aElt);

  NS_IMETHOD ResetElementFocus();

  // nsIDOMFocusListener
  NS_IMETHOD Focus(nsIDOMEvent* aEvent);
  NS_IMETHOD Blur(nsIDOMEvent* aEvent);

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* anEvent) { return NS_OK; }

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsFocusController,
                                           nsIFocusController)

protected:
  void UpdateCommands();
  void UpdateWWActiveWindow();

public:
  static nsPIDOMWindow *GetWindowFromDocument(nsIDOMDocument* aElement);

// Members
protected:
  nsCOMPtr<nsIDOMElement> mCurrentElement; // [OWNER]
  nsCOMPtr<nsPIDOMWindow> mCurrentWindow; // [OWNER]
  nsCOMPtr<nsIDOMNode> mPopupNode; // [OWNER]
  nsCOMPtr<nsIDOMEvent> mPopupEvent;

  PRUint32 mSuppressFocus;
  PRPackedBool mSuppressFocusScroll;
  PRPackedBool mActive;
  PRPackedBool mUpdateWindowWatcher;
  PRPackedBool mNeedUpdateCommands;
};

#endif // nsFocusController_h__
