/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s):
 *
 */

//
// nsPopupSetFrame
//

#ifndef nsPopupSetFrame_h__
#define nsPopupSetFrame_h__

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"

#include "nsIPopupSetFrame.h"
#include "nsBoxFrame.h"
#include "nsFrameList.h"
#include "nsIMenuParent.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsISupportsArray.h"

nsresult NS_NewPopupSetFrame(nsIPresShell* aPresShell, nsIFrame** aResult) ;

class nsCSSFrameConstructor;

class nsPopupSetFrame : public nsBoxFrame, public nsIPopupSetFrame
{
public:
  nsPopupSetFrame(nsIPresShell* aShell);

  NS_DECL_ISUPPORTS
  
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

    // nsIBox
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  NS_IMETHOD SetDebug(nsBoxLayoutState& aState, PRBool aDebug);

  // The following four methods are all overridden so that the menu children
  // can be stored in a separate list (so that they don't impact reflow of the
  // actual menu item at all).
  NS_IMETHOD FirstChild(nsIPresContext* aPresContext,
                        nsIAtom*        aListName,
                        nsIFrame**      aFirstChild) const;
  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom** aListName) const;
  NS_IMETHOD Destroy(nsIPresContext* aPresContext);

  // Reflow methods
  virtual void RePositionPopup(nsBoxLayoutState& aState);

  NS_IMETHOD  AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  InsertFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  RemoveFrame(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);

  NS_IMETHOD CreatePopup(nsIContent* aElementContent, nsIContent* aPopupContent, 
                         PRInt32 aXPos, PRInt32 aYPos, 
                         const nsString& aPopupType, const nsString& anAnchorAlignment,
                         const nsString& aPopupAlignment);

  NS_IMETHOD HidePopup();
  NS_IMETHOD DestroyPopup();
  NS_IMETHOD GetActiveChild(nsIDOMElement** aResult);
  NS_IMETHOD SetActiveChild(nsIDOMElement* aChild);

  PRBool OnCreate(nsIContent* aPopupContent);
  PRBool OnDestroy();

  void ActivatePopup(PRBool aActivateFlag);
  void OpenPopup(PRBool aOpenFlag);

  nsIFrame* GetActiveChild();
  void GetActiveChildElement(nsIContent** aResult);

  NS_IMETHOD GetFrameName(nsString& aResult) const
  {
      aResult.AssignWithConversion("PopupSet");
      return NS_OK;
  }

  void SetFrameConstructor(nsCSSFrameConstructor* aFC) {
    mFrameConstructor = aFC;
  }

protected:

  void MarkAsGenerated(nsIContent* aPopupContent);
  void UpdateDismissalListener(nsIMenuParent* aMenuParent);

protected:
  nsresult SetDebug(nsBoxLayoutState& aState, nsIFrame* aList, PRBool aDebug);

  nsFrameList mPopupFrames;
  nsIPresContext* mPresContext; // Our pres context.

  nsIContent* mElementContent; // The content that is having something popped up over it <weak>

  PRInt32 mXPos;                // Active child's x position
  PRInt32 mYPos;                // Active child's y position
  nsAutoString mPopupType;
  PRBool mCreateHandlerSucceeded;  // Did the create handler succeed?
  nsSize mLastPref;
private:
  nsCSSFrameConstructor* mFrameConstructor;
}; // class nsPopupSetFrame

#endif
