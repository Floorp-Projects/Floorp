/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 * 
 * The Original Code is XMLterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 */

// mozSimpleContainer.h: declaration of mozSimpleContainer class
// implementing mozISimpleContainer,
// which provides a WebShell container for use in simple programs
// using the layout engine

#include "nscore.h"
#include "nsCOMPtr.h"

#include "mozISimpleContainer.h"

class mozSimpleContainer : public mozISimpleContainer {

public:

  mozSimpleContainer();
  virtual ~mozSimpleContainer();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIWebShellContainer interface
  NS_IMETHOD WillLoadURL(nsIWebShell* aShell,
                         const PRUnichar* aURL,
                         nsLoadType aReason);
 
  NS_IMETHOD BeginLoadURL(nsIWebShell* aShell,
                          const PRUnichar* aURL);
 

  NS_IMETHOD ProgressLoadURL(nsIWebShell* aShell,
                             const PRUnichar* aURL,
                             PRInt32 aProgress,
                             PRInt32 aProgressMax);
 
  NS_IMETHOD EndLoadURL(nsIWebShell* aShell,
                        const PRUnichar* aURL,
                        nsresult aStatus);
 
  NS_IMETHOD NewWebShell(PRUint32 aChromeMask,
                         PRBool aVisible,
                         nsIWebShell *&aNewWebShell);
  
  NS_IMETHOD FindWebShellWithName(const PRUnichar* aName,
                                  nsIWebShell*& aResult);
  
  NS_IMETHOD ContentShellAdded(nsIWebShell* aChildShell,
                               nsIContent* frameNode);

  NS_IMETHOD CreatePopup(nsIDOMElement* aElement,
                         nsIDOMElement* aPopupContent, 
                         PRInt32 aXPos, PRInt32 aYPos, 
                         const nsString& aPopupType,
                         const nsString& anAnchorAlignment,
                         const nsString& aPopupAlignment,
                         nsIDOMWindow* aWindow, nsIDOMWindow** outPopup);

  NS_IMETHOD FocusAvailable(nsIWebShell* aFocusedWebShell,
                            PRBool& aFocusTaken);

  // other
  NS_IMETHOD Init(nsNativeWidget aNativeWidget,
                  PRInt32 width, PRInt32 height,
                  nsIPref* aPref);

  NS_IMETHOD Resize(PRInt32 aWidth, PRInt32 aHeight);

  NS_IMETHOD LoadURL(const char* aURL);

  NS_IMETHOD GetWebShell(nsIWebShell*& aWebShell);

  NS_IMETHOD GetDocument(nsIDOMDocument*& aDocument);

  NS_IMETHOD GetPresShell(nsIPresShell*& aPresShell);

protected:

  /** owning reference to web shell created by us */
  nsCOMPtr<nsIWebShell> mWebShell;

};
