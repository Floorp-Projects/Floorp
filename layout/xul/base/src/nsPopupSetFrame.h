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

struct nsPopupFrameList {
  nsPopupFrameList* mNextPopup;  // The next popup in the list.
  nsIFrame* mPopupFrame;         // Our popup.
  nsIContent* mPopupContent;     // The content element for the <popup> itself.
  
  nsIContent* mElementContent; // The content that is having something popped up over it <weak>

  PRInt32 mXPos;                // This child's x position
  PRInt32 mYPos;                // This child's y position

  nsAutoString mPopupAnchor;        // This child's anchor.
  nsAutoString mPopupAlign;         // This child's align.

  nsAutoString mPopupType;
  PRBool mCreateHandlerSucceeded;  // Did the create handler succeed?
  nsSize mLastPref;

public:
  nsPopupFrameList(nsIContent* aPopupContent, nsPopupFrameList* aNext);
  nsPopupFrameList* GetEntry(nsIContent* aPopupContent);
  nsPopupFrameList* GetEntryByFrame(nsIFrame* aPopupFrame);
};

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

  // Used to destroy our popup frames.
  NS_IMETHOD Destroy(nsIPresContext* aPresContext);

  // Reflow methods
  virtual void RepositionPopup(nsPopupFrameList* aEntry, nsBoxLayoutState& aState);

  NS_IMETHOD ShowPopup(nsIContent* aElementContent, nsIContent* aPopupContent, 
                       PRInt32 aXPos, PRInt32 aYPos, 
                       const nsString& aPopupType, const nsString& anAnchorAlignment,
                       const nsString& aPopupAlignment);
  NS_IMETHOD HidePopup(nsIFrame* aPopup);
  NS_IMETHOD DestroyPopup(nsIFrame* aPopup);

  NS_IMETHOD AddPopupFrame(nsIFrame* aPopup);
  NS_IMETHOD RemovePopupFrame(nsIFrame* aPopup);
  
  PRBool OnCreate(nsIContent* aPopupContent);
  PRBool OnDestroy(nsIContent* aPopupContent);
  PRBool OnCreated(nsIContent* aPopupContent);
  PRBool OnDestroyed(nsIContent* aPopupContent);

  void ActivatePopup(nsPopupFrameList* aEntry, PRBool aActivateFlag);
  void OpenPopup(nsPopupFrameList* aEntry, PRBool aOpenFlag);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const
  {
      return MakeFrameName("PopupSet", aResult);
  }
#endif

  void SetFrameConstructor(nsCSSFrameConstructor* aFC) {
    mFrameConstructor = aFC;
  }

protected:

  void MarkAsGenerated(nsIContent* aPopupContent);
  void UpdateDismissalListener(nsIMenuParent* aMenuParent);

protected:
  nsresult SetDebug(nsBoxLayoutState& aState, nsIFrame* aList, PRBool aDebug);

  nsPopupFrameList* mPopupList;
  
  nsIPresContext* mPresContext; // Our pres context.

private:
  nsCSSFrameConstructor* mFrameConstructor;
}; // class nsPopupSetFrame

#endif
