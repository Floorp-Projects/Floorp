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
#include "nsLineLayout.h"
#include "nsCSSLayout.h"
#include "nsStyleConsts.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIFontMetrics.h"

void
nsTextRun::List(FILE* out, PRInt32 aIndent)
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

nsLineLayout::nsLineLayout(nsIPresContext& aPresContext,
                           nsISpaceManager* aSpaceManager)
  : mPresContext(aPresContext)
{
  mSpaceManager = aSpaceManager;
  mListPositionOutside = PR_FALSE;
  mLineNumber = 0;
//  mLeftEdge = 0;
  mColumn = 0;
  mSkipLeadingWS = PR_TRUE;

  mTextRuns = nsnull;
  ResetTextRuns();
}

nsLineLayout::~nsLineLayout()
{
  nsTextRun::DeleteTextRuns(mTextRuns);
}

void
nsLineLayout::ResetTextRuns()
{
  nsTextRun::DeleteTextRuns(mTextRuns);
  mTextRuns = nsnull;
  mTextRunP = &mTextRuns;
  mNewTextRun = nsnull;
}

nsTextRun*
nsLineLayout::TakeTextRuns()
{
  nsTextRun* result = mTextRuns;
  mTextRuns = nsnull;
  ResetTextRuns();
  return result;
}

void
nsLineLayout::EndTextRun()
{
  mNewTextRun = nsnull;
}

nsresult
nsLineLayout::AddText(nsIFrame* aTextFrame)
{
  if (nsnull == mNewTextRun) {
    mNewTextRun = new nsTextRun();
    if (nsnull == mNewTextRun) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    *mTextRunP = mNewTextRun;
    mTextRunP = &mNewTextRun->mNext;
  }
  mNewTextRun->mArray.AppendElement(aTextFrame);
  return NS_OK;/* XXX */
}

// XXX move this somewhere else!!!
PRBool
nsLineLayout::TreatFrameAsBlock(const nsStyleDisplay* aDisplay,
                                const nsStylePosition* aPosition)
{
  if (NS_STYLE_POSITION_ABSOLUTE == aPosition->mPosition) {
    return PR_FALSE;
  }
  if (NS_STYLE_FLOAT_NONE != aDisplay->mFloats) {
    return PR_FALSE;
  }
  switch (aDisplay->mDisplay) {
  case NS_STYLE_DISPLAY_BLOCK:
  case NS_STYLE_DISPLAY_LIST_ITEM:
  case NS_STYLE_DISPLAY_TABLE:
    return PR_TRUE;
  }
  return PR_FALSE;
}
