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
  fprintf(out, "%p: count=%d <", this, n);
  for (i = 0; i < n; i++) {
    nsIFrame* text = (nsIFrame*) mArray.ElementAt(i);
    text->ListTag(out);
    printf(" ");
  }
  fputs(">\n", out);
}

//----------------------------------------------------------------------

nsCSSLineLayout::nsCSSLineLayout(nsIPresContext* aPresContext,
                                 nsISpaceManager* aSpaceManager)
{
  mPresContext = aPresContext;
  mSpaceManager = aSpaceManager;
  mListPositionOutside = PR_FALSE;
  mLineNumber = 0;
  mLeftEdge = 0;
  mColumn = 0;
  mSkipLeadingWS = PR_TRUE;

  mTextRuns = nsnull;
  ResetTextRuns();
}

nsCSSLineLayout::~nsCSSLineLayout()
{
  nsCSSTextRun::DeleteTextRuns(mTextRuns);
}

void
nsCSSLineLayout::ResetTextRuns()
{
  nsCSSTextRun::DeleteTextRuns(mTextRuns);
  mTextRuns = nsnull;
  mTextRunP = &mTextRuns;
  mNewTextRun = nsnull;
}

nsCSSTextRun*
nsCSSLineLayout::TakeTextRuns()
{
  nsCSSTextRun* result = mTextRuns;
  mTextRuns = nsnull;
  ResetTextRuns();
  return result;
}

void
nsCSSLineLayout::EndTextRun()
{
  mNewTextRun = nsnull;
}

nsresult
nsCSSLineLayout::AddText(nsIFrame* aTextFrame)
{
  if (nsnull == mNewTextRun) {
    mNewTextRun = new nsCSSTextRun();
    if (nsnull == mNewTextRun) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    *mTextRunP = mNewTextRun;
    mTextRunP = &mNewTextRun->mNext;
  }
  mNewTextRun->mArray.AppendElement(aTextFrame);
  return NS_OK;/* XXX */
}
