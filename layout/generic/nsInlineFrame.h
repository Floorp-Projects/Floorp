/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsInlineFrame_h___
#define nsInlineFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsAbsoluteContainingBlock.h"
#include "nsLineLayout.h"

class nsAnonymousBlockFrame;

#define NS_INLINE_FRAME_CID \
 { 0xa6cf90e0, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

#define nsInlineFrameSuper nsHTMLContainerFrame

#define NS_INLINE_FRAME_CONTAINS_PERCENT_AWARE_CHILD 0x00100000

/**
 * Inline frame class.
 *
 * This class manages a list of child frames that are inline frames. Working with
 * nsLineLayout, the class will reflow and place inline frames on a line.
 */
class nsInlineFrame : public nsInlineFrameSuper
{
public:
  friend nsresult NS_NewInlineFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  // nsISupports overrides
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame overrides
  NS_IMETHOD AppendFrames(nsIPresContext* aPresContext,
                          nsIPresShell& aPresShell,
                          nsIAtom* aListName,
                          nsIFrame* aFrameList);
  NS_IMETHOD InsertFrames(nsIPresContext* aPresContext,
                          nsIPresShell& aPresShell,
                          nsIAtom* aListName,
                          nsIFrame* aPrevFrame,
                          nsIFrame* aFrameList);
  NS_IMETHOD RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell& aPresShell,
                         nsIAtom* aListName,
                         nsIFrame* aOldFrame);
  NS_IMETHOD ReplaceFrame(nsIPresContext* aPresContext,
                          nsIPresShell& aPresShell,
                          nsIAtom* aListName,
                          nsIFrame* aOldFrame,
                          nsIFrame* aNewFrame);
  NS_IMETHOD ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
#endif
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

  // nsIHTMLReflow overrides
  NS_IMETHOD Reflow(nsIPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);

  NS_IMETHOD CanContinueTextRun(PRBool& aContinueTextRun) const;

  static nsIID kInlineFrameCID;

  // Take all of the frames away from this frame. The caller is
  // presumed to keep them alive.
  void StealAllFrames() {
    mFrames.SetFrames(nsnull);
  }

protected:
  // Additional reflow state used during our reflow methods
  struct InlineReflowState {
    nsIFrame* mNextRCFrame;
    nsIFrame* mPrevFrame;
    nsInlineFrame* mNextInFlow;
    PRPackedBool mSetParentPointer;  // when reflowing child frame first set its
                                     // parent frame pointer

    InlineReflowState()  {
      mNextRCFrame = nsnull;
      mPrevFrame = nsnull;
      mNextInFlow = nsnull;
      mSetParentPointer = PR_FALSE;
    };
  };

  nsInlineFrame();

  virtual PRIntn GetSkipSides() const;

  nsresult ReflowFrames(nsIPresContext* aPresContext,
                        const nsHTMLReflowState& aReflowState,
                        InlineReflowState& rs,
                        nsHTMLReflowMetrics& aMetrics,
                        nsReflowStatus& aStatus);

  nsresult ReflowInlineFrame(nsIPresContext* aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             InlineReflowState& rs,
                             nsIFrame* aFrame,
                             nsReflowStatus& aStatus);

  virtual nsIFrame* PullOneFrame(nsIPresContext* aPresContext,
                                 InlineReflowState& rs,
                                 PRBool* aIsComplete);

  virtual void PushFrames(nsIPresContext* aPresContext,
                          nsIFrame* aFromChild,
                          nsIFrame* aPrevSibling);

};

//----------------------------------------------------------------------

/**
 * Variation on inline-frame used to manage lines for line layout in
 * special situations (:first-line style in particular).
 */
class nsFirstLineFrame : public nsInlineFrame {
public:
  friend nsresult NS_NewFirstLineFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
#endif
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
  NS_IMETHOD Reflow(nsIPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);

  // Take frames starting at aFrame until the end of the frame-list
  // away from this frame. The caller is presumed to keep them alive.
  void StealFramesFrom(nsIFrame* aFrame);

protected:
  nsFirstLineFrame();

  virtual nsIFrame* PullOneFrame(nsIPresContext* aPresContext,
                                 InlineReflowState& rs,
                                 PRBool* aIsComplete);
};

//----------------------------------------------------------------------

// Derived class created for relatively positioned inline-level elements
// that acts as a containing block for child absolutely positioned
// elements

class nsPositionedInlineFrame : public nsInlineFrame
{
public:
  NS_IMETHOD Destroy(nsIPresContext* aPresContext);
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD AppendFrames(nsIPresContext* aPresContext,
                          nsIPresShell& aPresShell,
                          nsIAtom* aListName,
                          nsIFrame* aFrameList);
  NS_IMETHOD InsertFrames(nsIPresContext* aPresContext,
                          nsIPresShell& aPresShell,
                          nsIAtom* aListName,
                          nsIFrame* aPrevFrame,
                          nsIFrame* aFrameList);
  NS_IMETHOD RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell& aPresShell,
                         nsIAtom* aListName,
                         nsIFrame* aOldFrame);

  NS_IMETHOD GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom** aListName) const;

  NS_IMETHOD FirstChild(nsIPresContext* aPresContext,
                        nsIAtom*        aListName,
                        nsIFrame**      aFirstChild) const;

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

protected:
  nsAbsoluteContainingBlock mAbsoluteContainer;
};

#endif /* nsInlineFrame_h___ */
