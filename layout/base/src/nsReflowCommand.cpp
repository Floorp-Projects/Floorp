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
 */
#include "nsReflowCommand.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIRunaround.h"

static NS_DEFINE_IID(kIRunaroundIID, NS_IRUNAROUND_IID);

// Construct a reflow command given a presentation context, target frame,
// and a reflow command type
nsReflowCommand::nsReflowCommand(nsIPresContext* aPresContext,
                                 nsIFrame*       aTargetFrame,
                                 ReflowType      aReflowType)
  : mType(aReflowType), mTargetFrame(aTargetFrame), mPresContext(aPresContext),
    mChildFrame(nsnull)
{
  NS_PRECONDITION(mTargetFrame != nsnull, "null target frame");
  aPresContext->AddRef();
}

nsReflowCommand::nsReflowCommand(nsIPresContext* aPresContext,
                                 nsIFrame*       aTargetFrame,
                                 ReflowType      aReflowType,
                                 nsIFrame*       aChildFrame)
  : mType(aReflowType), mTargetFrame(aTargetFrame), mPresContext(aPresContext),
    mChildFrame(aChildFrame)
{
  NS_PRECONDITION(mTargetFrame != nsnull, "null target frame");
  NS_PRECONDITION(mChildFrame != nsnull, "null child frame");
  NS_ADDREF(aPresContext);
}

nsReflowCommand::~nsReflowCommand()
{
  NS_IF_RELEASE(mPresContext);
}

void nsReflowCommand::Dispatch(nsReflowMetrics& aDesiredSize,
                               const nsSize&    aMaxSize)
{
  // Build the path from the target frame (index 0) to the root frame
  mPath.Clear();
  for (nsIFrame* f = (nsIFrame*)mTargetFrame; nsnull != f;
       f->GetGeometricParent(f)) {
    mPath.AppendElement((void*)f);
  }

  // Send an incremental reflow notification to the root frame
  nsIFrame* root = (nsIFrame*)mPath[mPath.Count() - 1];

#ifdef NS_DEBUG
  nsIPresShell* shell = mPresContext->GetShell();
  if (nsnull != shell) {
    NS_ASSERTION(shell->GetRootFrame() == root, "bad root frame");
    NS_RELEASE(shell);
  }
#endif

  if (nsnull != root) {
    mPath.RemoveElementAt(mPath.Count() - 1);

    nsIFrame::ReflowStatus  status;

    root->IncrementalReflow(mPresContext, aDesiredSize, aMaxSize, *this, status);
  }
}

// Pass the reflow command to the next frame in the hierarchy
nsIFrame::ReflowStatus nsReflowCommand::Next(nsReflowMetrics& aDesiredSize,
                                             const nsSize&    aMaxSize,
                                             nsIFrame*&       aNextFrame)
{
  PRInt32                 count = mPath.Count();
  nsIFrame::ReflowStatus  result = nsIFrame::frComplete;

  NS_ASSERTION(count > 0, "empty path vector");
  if (count > 0) {
    aNextFrame = (nsIFrame*)mPath[count - 1];

    NS_ASSERTION(nsnull != aNextFrame, "null frame");
    mPath.RemoveElementAt(count - 1);
    aNextFrame->IncrementalReflow(mPresContext, aDesiredSize, aMaxSize,
                                  *this, result);
  } else {
    aNextFrame = nsnull;
  }

  return result;
}

// Pass the reflow command to the next frame in the hierarchy. Check
// whether it wants to use nsIRunaround or nsIFrame
nsIFrame::ReflowStatus nsReflowCommand::Next(nsISpaceManager* aSpaceManager,
                                             nsRect&          aDesiredRect,
                                             const nsSize&    aMaxSize,
                                             nsIFrame*&       aNextFrame)
{
  PRInt32                 count = mPath.Count();
  nsIFrame::ReflowStatus  result = nsIFrame::frComplete;

  NS_ASSERTION(count > 0, "empty path vector");
  if (count > 0) {
    aNextFrame = (nsIFrame*)mPath[count - 1];

    NS_ASSERTION(nsnull != aNextFrame, "null frame");
    mPath.RemoveElementAt(count - 1);

    // Does the frame support nsIRunaround?
    nsIRunaround* reflowRunaround;

    if (NS_OK == aNextFrame->QueryInterface(kIRunaroundIID, (void**)&reflowRunaround)) {
      reflowRunaround->IncrementalReflow(mPresContext, aSpaceManager, aMaxSize,
                                         aDesiredRect, *this, result);
    } else {
      nsReflowMetrics desiredSize;

      aNextFrame->IncrementalReflow(mPresContext, desiredSize, aMaxSize,
                                    *this, result);
      aDesiredRect.x = 0;
      aDesiredRect.y = 0;
      aDesiredRect.width = desiredSize.width;
      aDesiredRect.height = desiredSize.height;
    }
  } else {
    aNextFrame = nsnull;
  }

  return result;
}

nsIFrame* nsReflowCommand::GetNext() const
{
  PRInt32 count = mPath.Count();
  nsIFrame* rv = nsnull;
  if (count > 0) {
    rv = (nsIFrame*) mPath[count - 1];
  }
  return rv;
}
