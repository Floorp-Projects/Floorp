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

// mozISimpleContainer.h: a simple WebShell container interface
//                        for use in simple programs using the layout engine
//                        (unregistered interface)

#ifndef mozISimpleContainer_h___
#define mozISimpleContainer_h___

#include "nscore.h"

#include "nsISupports.h"
#include "nsIWebShell.h"
#include "nsIPref.h"
#include "nsIPresShell.h"
#include "nsIDOMDocument.h"

/* starting interface: mozISimpleContainer */

/* {0eb82bF0-43a2-11d3-8e76-006008948af5} */
#define MOZISIMPLE_CONTAINER_IID_STR "0eb82bF0-43a2-11d3-8e76-006008948af5"
#define MOZISIMPLE_CONTAINER_IID \
  {0x0eb82bF0, 0x43a2, 0x11d3, \
    { 0x8e, 0x76, 0x00, 0x60, 0x08, 0x94, 0x8a, 0xf5 }}

class mozISimpleContainer : public nsIWebShellContainer {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(MOZISIMPLE_CONTAINER_IID)

  // nsIWebShellContainer interface
  NS_IMETHOD WillLoadURL(nsIWebShell* aShell,
                         const PRUnichar* aURL,
                         nsLoadType aReason) = 0;
 
  NS_IMETHOD BeginLoadURL(nsIWebShell* aShell,
                          const PRUnichar* aURL) = 0;
 

  NS_IMETHOD ProgressLoadURL(nsIWebShell* aShell,
                             const PRUnichar* aURL,
                             PRInt32 aProgress,
                             PRInt32 aProgressMax) = 0;
 
  NS_IMETHOD EndLoadURL(nsIWebShell* aShell,
                        const PRUnichar* aURL,
                        nsresult aStatus) = 0;
 
  NS_IMETHOD NewWebShell(PRUint32 aChromeMask,
                         PRBool aVisible,
                         nsIWebShell *&aNewWebShell) = 0;
  
  NS_IMETHOD FindWebShellWithName(const PRUnichar* aName,
                                  nsIWebShell*& aResult) = 0;
  
  NS_IMETHOD ContentShellAdded(nsIWebShell* aChildShell,
                               nsIContent* frameNode) = 0;

  NS_IMETHOD CreatePopup(nsIDOMElement* aElement,
                         nsIDOMElement* aPopupContent, 
                         PRInt32 aXPos, PRInt32 aYPos, 
                         const nsString& aPopupType,
                         const nsString& anAnchorAlignment,
                         const nsString& aPopupAlignment,
                         nsIDOMWindow* aWindow, nsIDOMWindow** outPopup) = 0;

  NS_IMETHOD FocusAvailable(nsIWebShell* aFocusedWebShell,
                            PRBool& aFocusTaken) = 0;

  // other

  /** Initializes simple container for native window widget
   * @param aNativeWidget native window widget (e.g., GtkWidget)
   * @param width window width (pixels)
   * @param height window height (pixels)
   * @param aPref preferences object
   */
  NS_IMETHOD Init(nsNativeWidget aNativeWidget,
                  PRInt32 width, PRInt32 height,
                  nsIPref* aPref) = 0;

  /** Resizes container to new dimensions
   * @param width new window width (pixels)
   * @param height new window height (pixels)
   */
  NS_IMETHOD Resize(PRInt32 aWidth, PRInt32 aHeight) = 0;

  /** Loads specified URL into container
   * @param aURL URL string
   */
  NS_IMETHOD LoadURL(const char* aURL) = 0;

  /** Gets web shell in container
   * @param aWebShell (output) web shell object
   */
  NS_IMETHOD GetWebShell(nsIWebShell*& aWebShell) = 0;

  /** Gets DOM document in container
   * @param aDocument (output) DOM document
   */
  NS_IMETHOD GetDocument(nsIDOMDocument*& aDocument) = 0;

  /** Gets presentation shell associated with container
   * @param aPresShell (output) presentation shell
   */
  NS_IMETHOD GetPresShell(nsIPresShell*& aPresShell) = 0;
};

#define MOZSIMPLE_CONTAINER_CID              \
{ /* 0eb82bF1-43a2-11d3-8e76-006008948af5 */ \
   0x0eb82bF1, 0x43a2, 0x11d3,               \
{0x8e, 0x76, 0x00, 0x60, 0x08, 0x94, 0x8a, 0xf5} }
extern nsresult
NS_NewSimpleContainer(mozISimpleContainer** aSimpleContainer);

#endif /* mozISimpleContainer_h___ */
