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
#include "nsCSSLineLayout.h"
#include "nsCSSLayout.h"

#if 0
#include "nsIFontMetrics.h"
#include "nsIPresContext.h"
#include "nsIRunaround.h"
#include "nsIStyleContext.h"

// XXX nsCSSIIDs.[h,cpp]
static NS_DEFINE_IID(kIInlineReflowIID, NS_IINLINE_REFLOW_IID);
static NS_DEFINE_IID(kIRunaroundIID, NS_IRUNAROUND_IID);
#endif

void
nsCSSTextRun::List(FILE* out, PRInt32 aIndent)
{
  PRInt32 i;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  PRInt32 n = mArray.Count();
  fprintf(out, "count=%d <\n", n);
  for (i = 0; i < n; i++) {
    nsIFrame* text = (nsIFrame*) mArray.ElementAt(i);
    text->List(out, aIndent + 1);
  }
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs(">\n", out);
}

//----------------------------------------------------------------------

nsCSSLineLayout::nsCSSLineLayout(nsIPresContext* aPresContext,
                                 nsISpaceManager* aSpaceManager)
{
  mPresContext = aPresContext;
  mSpaceManager = aSpaceManager;
  mTextRuns = nsnull;
  mTextRunP = &mTextRuns;
  mCurrentTextRun = nsnull;
  mListPositionOutside = PR_FALSE;
  mLineNumber = 0;
  mLeftEdge = 0;
  mColumn = 0;
  mSkipLeadingWS = PR_TRUE;
}

nsCSSLineLayout::~nsCSSLineLayout()
{
  if (nsnull != mTextRuns) {
    delete mTextRuns;
  }
}

void
nsCSSLineLayout::EndTextRun()
{
  if (nsnull != mCurrentTextRun) {
    // Keep the text-run if it's not empty
    if (mCurrentTextRun->mArray.Count() > 0) {
      *mTextRunP = mCurrentTextRun;
      mTextRunP = &mCurrentTextRun->mNext;
    }
    else {
      delete mCurrentTextRun;
    }
    mCurrentTextRun = nsnull;
  }
}

nsresult
nsCSSLineLayout::AddText(nsIFrame* aTextFrame)
{
  if (nsnull == mCurrentTextRun) {
    mCurrentTextRun = new nsCSSTextRun();
  }
  mCurrentTextRun->mArray.AppendElement(aTextFrame);
  return NS_OK;/* XXX */
}
