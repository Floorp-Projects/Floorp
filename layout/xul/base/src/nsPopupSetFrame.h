/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "nsISupportsArray.h"

nsIFrame* NS_NewPopupSetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

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
  PRPackedBool mCreateHandlerSucceeded;  // Did the create handler succeed?
  PRPackedBool mIsOpen;
  nsSize mLastPref;

public:
  nsPopupFrameList(nsIContent* aPopupContent, nsPopupFrameList* aNext);
  nsPopupFrameList* GetEntry(nsIContent* aPopupContent);
  nsPopupFrameList* GetEntryByFrame(nsIFrame* aPopupFrame);
};

class nsPopupSetFrame : public nsBoxFrame, public nsIPopupSetFrame
{
public:
  nsPopupSetFrame(nsIPresShell* aShell, nsStyleContext* aContext):
    nsBoxFrame(aShell, aContext) {}

  NS_DECL_ISUPPORTS
  
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD AppendFrames(nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  NS_IMETHOD RemoveFrame(nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);
  NS_IMETHOD InsertFrames(nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);
  NS_IMETHOD  SetInitialChildList(nsIAtom*        aListName,
                                  nsIFrame*       aChildList);
  
    // nsIBox
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
#ifdef DEBUG_LAYOUT
  NS_IMETHOD SetDebug(nsBoxLayoutState& aState, PRBool aDebug);
#endif

  // Used to destroy our popup frames.
  virtual void Destroy();

  // Reflow methods
  virtual void RepositionPopup(nsPopupFrameList* aEntry, nsBoxLayoutState& aState);

  NS_IMETHOD ShowPopup(nsIContent* aElementContent, nsIContent* aPopupContent, 
                       PRInt32 aXPos, PRInt32 aYPos, 
                       const nsString& aPopupType, const nsString& anAnchorAlignment,
                       const nsString& aPopupAlignment);
  NS_IMETHOD HidePopup(nsIFrame* aPopup);
  NS_IMETHOD DestroyPopup(nsIFrame* aPopup, PRBool aDestroyEntireChain);

  PRBool OnCreate(PRInt32 aX, PRInt32 aY, nsIContent* aPopupContent);
  PRBool OnDestroy(nsIContent* aPopupContent);
  PRBool OnCreated(PRInt32 aX, PRInt32 aY, nsIContent* aPopupContent);
  static PRBool OnDestroyed(nsPresContext* aPresContext,
                            nsIContent* aPopupContent);

  void ActivatePopup(nsPopupFrameList* aEntry, PRBool aActivateFlag);
  void OpenPopup(nsPopupFrameList* aEntry, PRBool aOpenFlag);

  /**
   * Return true if the docshell containing aFrame may open a popup. aFrame
   * doesn't need to be any particular type of frame, just a frame in the
   * same document.
   */
  static PRBool MayOpenPopup(nsIFrame* aFrame);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
      return MakeFrameName(NS_LITERAL_STRING("PopupSet"), aResult);
  }
#endif

protected:

  nsresult AddPopupFrameList(nsIFrame* aPopupFrameList);
  nsresult AddPopupFrame(nsIFrame* aPopup);
  nsresult RemovePopupFrame(nsIFrame* aPopup);
  
  void MarkAsGenerated(nsIContent* aPopupContent);

protected:
#ifdef DEBUG_LAYOUT
  nsresult SetDebug(nsBoxLayoutState& aState, nsIFrame* aList, PRBool aDebug);
#endif

  nsPopupFrameList* mPopupList;

}; // class nsPopupSetFrame

#endif
