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
#include "nsGenericAccessible.h"
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

class nsAccessible : public nsGenericAccessible
{
public:
  NS_IMETHOD GetAccName(nsAWritableString& _retval);
  NS_IMETHOD GetAccParent(nsIAccessible **_retval); 
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval); 
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval); 
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval); 
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval); 
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval); 
  NS_IMETHOD GetAccState(PRUint32 *_retval); 
  NS_IMETHOD GetAccFocused(nsIAccessible **_retval); 
  NS_IMETHOD AccGetAt(PRInt32 x, PRInt32 y, nsIAccessible **_retval); 
  NS_IMETHOD AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height); 
  NS_IMETHOD AccRemoveSelection(void); 
  NS_IMETHOD AccTakeSelection(void); 
  NS_IMETHOD AccTakeFocus(void); 
  NS_IMETHOD AccGetDOMNode(nsIDOMNode **_retval); 

  public:
    nsAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
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

    // helper method to verify frames
    static PRBool IsCorrectFrameType(nsIFrame* aFrame, nsIAtom* aAtom);

protected:
  virtual nsIFrame* GetFrame();
  virtual nsIFrame* GetBoundsFrame();
  virtual void GetBounds(nsRect& aRect, nsIFrame** aRelativeFrame);
  virtual void GetPresContext(nsCOMPtr<nsIPresContext>& aContext);
  NS_IMETHOD AppendFlatStringFromSubtree(nsIContent *aContent, nsAWritableString *aFlatString);
  NS_IMETHOD AppendFlatStringFromContentNode(nsIContent *aContent, nsAWritableString *aFlatString);

  // Data Members
  nsCOMPtr<nsIDOMNode> mDOMNode;
  nsCOMPtr<nsIWeakReference> mPresShell;
  nsCOMPtr<nsIFocusController> mFocusController;

};

/* Special Accessible that knows how to handle hit detection for flowing text */
class nsHTMLBlockAccessible : public nsAccessible
{
public:
   nsHTMLBlockAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
   NS_IMETHOD AccGetAt(PRInt32 x, PRInt32 y, nsIAccessible **_retval);
};

/* Leaf version of DOM Accessible
 * has no children
 */
class nsLeafAccessible : public nsAccessible
{
  public:
    nsLeafAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);

    NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
    NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
    NS_IMETHOD GetAccChildCount(PRInt32 *_retval);
};


class nsLinkableAccessible : public nsAccessible
{
  public:
    nsLinkableAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
    NS_IMETHOD GetAccNumActions(PRUint8 *_retval);
    NS_IMETHOD GetAccActionName(PRUint8 index, nsAWritableString& _retval);
    NS_IMETHOD AccDoAction(PRUint8 index);
    NS_IMETHOD GetAccState(PRUint32 *_retval);
    NS_IMETHOD GetAccValue(nsAWritableString& _retval);

  protected:
    PRBool IsALink();
    PRBool mIsALinkCached;  // -1 = unknown, 0 = not a link, 1 = is a link
    nsCOMPtr<nsIContent> mLinkContent;
    PRBool mIsLinkVisited;
};


#endif  
