/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsLineLayout_h___
#define nsLineLayout_h___

#include "nsIFrame.h"
#include "nsVoidArray.h"

struct nsBlockReflowState;
class nsInlineReflow;
class nsPlaceholderFrame;
struct nsStyleDisplay;
struct nsStylePosition;
struct nsStyleSpacing;

// This structure represents a run of text. In mText are the
// nsIFrame's that are considered text frames.
struct nsTextRun {
  nsTextRun() {
    mNext = nsnull;
  }

  static void DeleteTextRuns(nsTextRun* aRun) {
    while (nsnull != aRun) {
      nsTextRun* next = aRun->mNext;
      delete aRun;
      aRun = next;
    }
  }

  void List(FILE* out, PRInt32 aIndent);

  nsVoidArray mArray;
  nsTextRun* mNext;

protected:
  ~nsTextRun() {
  }
};

//----------------------------------------------------------------------

class nsLineLayout {
public:
  nsLineLayout(nsIPresContext& aPresContext,
               nsISpaceManager* aSpaceManager);
  ~nsLineLayout();

  void Init(nsBlockReflowState* aReflowState) {/* XXX ctor arg really */
    mBlockReflowState = aReflowState;
  }

  // Prepare this line-layout for the reflow of a new line
  void Reset() {
    mTotalPlacedFrames = 0;
    mColumn = 0;
    mSkipLeadingWS = PR_TRUE;
    mBRFrame = nsnull;
#ifdef NS_DEBUG
    mPlacedFrames.Clear();
#endif
    ForgetWordFrames();
  }

  // Add to the placed-frame count
#ifdef NS_DEBUG
  void AddPlacedFrame(nsIFrame* aFrame) {
    mTotalPlacedFrames++;
    mPlacedFrames.AppendElement(aFrame);
  }
#else
  void AddPlacedFrame() {
    mTotalPlacedFrames++;
  }
#endif

  // Get the placed-frame count
  PRInt32 GetPlacedFrames() const {
    return mTotalPlacedFrames;
  }

  void SetBRFrame(nsIFrame* aFrame) {
    mBRFrame = aFrame;
  }

  nsIFrame* GetBRFrame() const {
    return mBRFrame;
  }

  void PushInline(nsInlineReflow* aInlineReflow) {
    mInlineStack.AppendElement(aInlineReflow);
  }

  void PopInline() {
    PRInt32 n = mInlineStack.Count();
    if (n > 0) {
      mInlineStack.RemoveElementAt(n - 1);
    }
  }

  void UpdateInlines(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                     PRBool aPlacedLeftFloater);

  // Reset the text-run information in preparation for a FindTextRuns
  void ResetTextRuns();

  // Add another piece of text to a text-run during FindTextRuns.
  // Note: continuation frames must NOT add themselves; just the
  // first-in-flow
  nsresult AddText(nsIFrame* aTextFrame);

  // Close out a text-run during FindTextRuns.
  void EndTextRun();

  // This returns the first nsTextRun found during a
  // FindTextRuns. The internal text-run state is reset.
  nsTextRun* TakeTextRuns();

  void SetReflowTextRuns(nsTextRun* aTextRuns) {
    mReflowTextRuns = aTextRuns;
  }

  nsIFrame* FindNextText(nsIFrame* aFrame);

  void RecordWordFrame(nsIFrame* aWordFrame) {
    mWordFrames.AppendElement(aWordFrame);
  }

  void ForgetWordFrames() {
    mWordFrames.Clear();
  }

  PRBool IsNextWordFrame(nsIFrame* aFrame);

  PRBool InNonBreakingUnit() {
    return 0 != mWordFrames.Count();
  }

  PRBool IsLastWordFrame(nsIFrame* aFrame);

  void ForgetWordFrame(nsIFrame* aFrame);

  PRInt32 GetColumn() {
    return mColumn;
  }

  void SetColumn(PRInt32 aNewColumn) {
    mColumn = aNewColumn;
  }

  void NextLine() {
    mLineNumber++;
  }
  
  PRInt32 GetLineNumber() const {
    return mLineNumber;
  }

  static PRBool TreatFrameAsBlock(const nsStyleDisplay* aDisplay,
                                  const nsStylePosition* aPosition);

  //  --------------------------------------------------

  void AddFloater(nsPlaceholderFrame* aFrame);

  nsIPresContext& mPresContext;
  nsISpaceManager* mSpaceManager;
  nsBlockReflowState* mBlockReflowState;

  PRBool mListPositionOutside;
  PRInt32 mLineNumber;
  PRInt32 mColumn;

  //XXX temporary?

  void SetSkipLeadingWhiteSpace(PRBool aNewSetting) {
    mSkipLeadingWS = aNewSetting;
  }
  PRBool GetSkipLeadingWhiteSpace() { return mSkipLeadingWS; }

  PRBool mSkipLeadingWS;

protected:
  nsIFrame* mBRFrame;

  PRInt32 mTotalPlacedFrames;
#ifdef NS_DEBUG
  nsVoidArray mPlacedFrames;
#endif

  nsVoidArray mWordFrames;

  nsVoidArray mInlineStack;

  // These slots are used during FindTextRuns
  nsTextRun* mTextRuns;
  nsTextRun** mTextRunP;
  nsTextRun* mNewTextRun;

  // These slots are used during InlineReflow
  nsTextRun* mReflowTextRuns;
  nsTextRun* mTextRun;
};

#endif /* nsLineLayout_h___ */
