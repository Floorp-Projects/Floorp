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
#ifndef nsCSSLineLayout_h___
#define nsCSSLineLayout_h___

#include "nsIFrame.h"
#include "nsVoidArray.h"

// This structure represents a run of text. In mText are the
// nsIFrame's that are considered text frames.
struct nsCSSTextRun {
  nsCSSTextRun() {
    mNext = nsnull;
  }

  static void DeleteTextRuns(nsCSSTextRun* aRun) {
    while (nsnull != aRun) {
      nsCSSTextRun* next = aRun->mNext;
      delete aRun;
      aRun = next;
    }
  }

  void List(FILE* out, PRInt32 aIndent);

  nsVoidArray mArray;
  nsCSSTextRun* mNext;

protected:
  ~nsCSSTextRun() {
  }
};

//----------------------------------------------------------------------

class nsCSSLineLayout {
public:
  nsCSSLineLayout(nsIPresContext* aPresContext,
                  nsISpaceManager* aSpaceManager);
  ~nsCSSLineLayout();

  // Reset the text-run information in preparation for a FindTextRuns
  void ResetTextRuns();

  // Add another piece of text to a text-run during FindTextRuns.
  // Note: continuation frames must NOT add themselves; just the
  // first-in-flow
  nsresult AddText(nsIFrame* aTextFrame);

  // Close out a text-run during FindTextRuns.
  void EndTextRun();

  // This returns the first nsCSSTextRun found during a
  // FindTextRuns. The internal text-run state is reset.
  nsCSSTextRun* TakeTextRuns();

  // Prepare this line-layout for the reflow of a new line
  void Prepare(nscoord aLeftEdge) {
    mLeftEdge = aLeftEdge;
    mColumn = 0;
    mSkipLeadingWS = PR_TRUE;
  }

  PRInt32 GetColumn() {
    return mColumn;
  }

  void SetColumn(PRInt32 aNewColumn) {
    mColumn = aNewColumn;
  }

  void NextLine() {
    mLineNumber++;
  }

  nsIPresContext* mPresContext;
  nsISpaceManager* mSpaceManager;

  PRBool mListPositionOutside;
  PRInt32 mLineNumber;
  nscoord mLeftEdge;
  PRInt32 mColumn;

  //XXX temporary?

  void SetSkipLeadingWhiteSpace(PRBool aNewSetting) {
    mSkipLeadingWS = aNewSetting;
  }
  PRBool GetSkipLeadingWhiteSpace() { return mSkipLeadingWS; }

  PRBool mSkipLeadingWS;

protected:
  // These slots are used during FindTextRuns
  nsCSSTextRun* mTextRuns;
  nsCSSTextRun** mTextRunP;
  nsCSSTextRun* mNewTextRun;

  // These slots are used during InlineReflow
  nsCSSTextRun* mReflowTextRuns;
  nsCSSTextRun* mTextRun;
};

#endif /* nsCSSLineLayout_h___ */
