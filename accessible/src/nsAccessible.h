/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _nsAccessible_H_
#define _nsAccessible_H_

#include "nsISupports.h"
#include "nsIAccessible.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsWeakReference.h"
#include "nsIFocusController.h"
#include "nsRect.h"
#include "nsPoint.h"

#define ACCESSIBLE_BUNDLE_URL "chrome://global/locale/accessible.properties"

class nsIFrame;
class nsIDocShell;
class nsIWebShell;
class nsIContent;

class nsAccessible : public nsIAccessible
//                     public nsIAccessibleWidgetAccess
{
  NS_DECL_ISUPPORTS

  // nsIAccessibilityService methods:
  NS_DECL_NSIACCESSIBLE

  //NS_IMETHOD AccGetWidget(nsIWidget**);

  public:
    nsAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell);
    virtual ~nsAccessible();

    virtual void GetListAtomForFrame(nsIFrame* aFrame, nsIAtom*& aList) { aList = nsnull; }

    // Helper Routines for Sub-Docs
    static nsresult GetDocShellFromPS(nsIPresShell* aPresShell, nsIDocShell** aDocShell);
    static nsresult GetDocShellObjects(nsIDocShell*     aDocShell,
                                nsIPresShell**   aPresShell, 
                                nsIPresContext** aPresContext, 
                                nsIContent**     aContent);
    static nsresult GetDocShells(nsIPresShell* aPresShell,
                          nsIDocShell** aDocShell,
                          nsIDocShell** aParentDocShell);
    static nsresult GetParentPresShellAndContent(nsIPresShell*  aPresShell,
                                          nsIPresShell** aParentPresShell,
                                          nsIContent**   aSubShellContent);

    static PRBool FindContentForWebShell(nsIPresShell* aParentPresShell,
                                  nsIContent*   aParentContent,
                                  nsIWebShell*  aWebShell,
                                  nsIContent**  aFoundContent);
    nsresult CalcOffset(nsIFrame* aFrame,
                        nsIPresContext * aPresContext,
                        nsRect& aRect);
    nsresult GetAbsPosition(nsIPresShell* aPresShell, nsPoint& aPoint);
    nsresult GetAbsoluteFramePosition(nsIPresContext* aPresContext,
                                      nsIFrame *aFrame, 
                                      nsRect& aAbsoluteTwipsRect, 
                                      nsRect& aAbsolutePixelRect);
    static nsresult GetTranslatedString(PRUnichar *aKey, nsAWritableString *aStringOut);
protected:
  virtual nsIFrame* GetFrame();
  virtual nsIFrame* GetBoundsFrame();
  virtual void GetBounds(nsRect& aRect);
  virtual void GetPresContext(nsCOMPtr<nsIPresContext>& aContext);
  virtual nsIAccessible* CreateNewNextAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell);
  virtual nsIAccessible* CreateNewPreviousAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell);
  virtual nsIAccessible* CreateNewParentAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell);
  virtual nsIAccessible* CreateNewFirstAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell);
  virtual nsIAccessible* CreateNewLastAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell);
  virtual nsIAccessible* CreateNewAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell);

  // Data Members
  nsCOMPtr<nsIDOMNode> mDOMNode;
  nsCOMPtr<nsIWeakReference> mPresShell;
  nsCOMPtr<nsIAccessible> mAccessible;
  nsCOMPtr<nsIFocusController> mFocusController;
};

/* Special Accessible that knows how to handle hit detection for flowing text */
class nsHTMLBlockAccessible : public nsAccessible
{
public:
   nsHTMLBlockAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell);
   NS_IMETHOD AccGetAt(PRInt32 x, PRInt32 y, nsIAccessible **_retval);
protected:
   virtual nsIAccessible* CreateNewAccessible(nsIAccessible* aAccessible, nsIDOMNode* aFrame, nsIWeakReference* aShell);
};

#endif  
