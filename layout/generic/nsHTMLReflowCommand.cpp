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
#include "nsHTMLReflowCommand.h"
#include "nsIFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIFloaterContainer.h"
#include "nsHTMLIIDs.h"

static NS_DEFINE_IID(kIReflowCommandIID, NS_IREFLOWCOMMAND_IID);

nsresult
NS_NewHTMLReflowCommand(nsIReflowCommand**           aInstancePtrResult,
                        nsIFrame*                    aTargetFrame,
                        nsIReflowCommand::ReflowType aReflowType,
                        nsIFrame*                    aChildFrame)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsHTMLReflowCommand* cmd = new nsHTMLReflowCommand(aTargetFrame, aReflowType, aChildFrame);
  if (nsnull == cmd) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return cmd->QueryInterface(kIReflowCommandIID, (void**)aInstancePtrResult);
}


// Construct a reflow command given a target frame, reflow command type,
// and optional child frame
nsHTMLReflowCommand::nsHTMLReflowCommand(nsIFrame*  aTargetFrame,
                                         ReflowType aReflowType,
                                         nsIFrame*  aChildFrame)
  : mType(aReflowType), mTargetFrame(aTargetFrame), mChildFrame(aChildFrame)
{
  NS_PRECONDITION(mTargetFrame != nsnull, "null target frame");
  NS_INIT_REFCNT();
}

nsHTMLReflowCommand::~nsHTMLReflowCommand()
{
}

NS_IMPL_ISUPPORTS(nsHTMLReflowCommand, kIReflowCommandIID);

nsIFrame* nsHTMLReflowCommand::GetContainingBlock(nsIFrame* aFloater)
{
  nsIFrame*             containingBlock = nsnull;
  nsIFloaterContainer*  container = nsnull;

  for (aFloater->GetContentParent(containingBlock);
       nsnull != containingBlock;
       containingBlock->GetContentParent(containingBlock))
  {
    if (NS_OK == containingBlock->QueryInterface(kIFloaterContainerIID, (void**)&container)) {
      break;
    }
  }

  return containingBlock;
}

void nsHTMLReflowCommand::BuildPath()
{
  mPath.Clear();

  // Floating frames are handled differently. The path goes from the target
  // frame to the containing block, and then up the hierarchy
  nsStyleDisplay* display;
  mTargetFrame->GetStyleData(eStyleStruct_Display, (nsStyleStruct*&)display);
  if (NS_STYLE_FLOAT_NONE != display->mFloats) {
    mPath.AppendElement((void*)mTargetFrame);

    for (nsIFrame* f = GetContainingBlock(mTargetFrame);
         nsnull != f;
         f->GetGeometricParent(f)) {
      mPath.AppendElement((void*)f);
    }

  } else {
    for (nsIFrame* f = mTargetFrame; nsnull != f; f->GetGeometricParent(f)) {
      mPath.AppendElement((void*)f);
    }
  }
}

NS_IMETHODIMP nsHTMLReflowCommand::Dispatch(nsIPresContext&  aPresContext,
                                            nsReflowMetrics& aDesiredSize,
                                            const nsSize&    aMaxSize)
{
  // Build the path from the target frame (index 0) to the root frame
  BuildPath();

  // Send an incremental reflow notification to the root frame
  nsIFrame* root = (nsIFrame*)mPath[mPath.Count() - 1];

#ifdef NS_DEBUG
  nsIPresShell* shell = aPresContext.GetShell();
  if (nsnull != shell) {
    NS_ASSERTION(shell->GetRootFrame() == root, "bad root frame");
    NS_RELEASE(shell);
  }
#endif

  if (nsnull != root) {
    mPath.RemoveElementAt(mPath.Count() - 1);

    nsReflowState   reflowState(root, *this, aMaxSize);
    nsReflowStatus  status;
    root->Reflow(&aPresContext, aDesiredSize, reflowState, status);
  }

  return NS_OK;
}

NS_IMETHODIMP nsHTMLReflowCommand::GetNext(nsIFrame*& aNextFrame)
{
  PRInt32 count = mPath.Count();

  aNextFrame = nsnull;
  if (count > 0) {
    aNextFrame = (nsIFrame*)mPath[count - 1];
    mPath.RemoveElementAt(count - 1);
  }
  return NS_OK;
}

NS_IMETHODIMP nsHTMLReflowCommand::GetTarget(nsIFrame*& aTargetFrame) const
{
  aTargetFrame = mTargetFrame;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLReflowCommand::GetType(ReflowType& aReflowType) const
{
  aReflowType = mType;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLReflowCommand::GetChildFrame(nsIFrame*& aChildFrame) const
{
  aChildFrame = mChildFrame;
  return NS_OK;
}

