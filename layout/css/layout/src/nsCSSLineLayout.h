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

  ~nsCSSTextRun() {
    nsCSSTextRun* run = mNext;
    while (nsnull != run) {
      nsCSSTextRun* next = run->mNext;
      delete run;
      run = next;
    }
  }

  void List(FILE* out, PRInt32 aIndent);

  nsVoidArray mArray;
  nsCSSTextRun* mNext;
};

//----------------------------------------------------------------------

// 1) collecting frames for text-runs
// 2) horizontal layout of frames for block
// 3) horizontal layout of frames for inline
// 4) state storage for text-reflow

class nsCSSLineLayout {
public:
  nsCSSLineLayout(nsIPresContext* aPresContext,
                  nsISpaceManager* aSpaceManager);
  ~nsCSSLineLayout();

  void EndTextRun();

  // Note: continuation frames must NOT add themselves; just the
  // first-in-flow
  nsresult AddText(nsIFrame* aTextFrame);

  nsCSSTextRun* TakeTextRuns() {
    nsCSSTextRun* result = mTextRuns;
    mTextRuns = nsnull;
    return result;
  }

  void Prepare(nscoord aLeftEdge) {
    mLeftEdge = aLeftEdge;
    mColumn = 0;
  }

  PRInt32 GetColumn() {
    return mColumn;
  }

  void SetColumn(PRInt32 aNewColumn) {
    mColumn = aNewColumn;
  }

  nsIPresContext* mPresContext;
  nsISpaceManager* mSpaceManager;

  PRBool mListPositionOutside;
  PRInt32 mLineNumber;
  nscoord mLeftEdge;
  PRInt32 mColumn;

  nsCSSTextRun* mTextRuns;
  nsCSSTextRun** mTextRunP;
  nsCSSTextRun* mCurrentTextRun;
};

#endif /* nsCSSLineLayout_h___ */
