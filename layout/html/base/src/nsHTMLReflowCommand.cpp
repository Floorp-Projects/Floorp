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
 * The Original Code is mozilla.org code.
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
#include "nsCOMPtr.h"
#include "nsHTMLReflowCommand.h"
#include "nsHTMLParts.h"
#include "nsIFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsHTMLIIDs.h"
#include "nsFrame.h"
#include "nsContainerFrame.h"


nsresult
NS_NewHTMLReflowCommand(nsIReflowCommand**           aInstancePtrResult,
                        nsIFrame*                    aTargetFrame,
                        nsIReflowCommand::ReflowType aReflowType,
                        nsIFrame*                    aChildFrame,
                        nsIAtom*                     aAttribute)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsHTMLReflowCommand* cmd = new nsHTMLReflowCommand(aTargetFrame, aReflowType, aChildFrame, aAttribute);
  if (nsnull == cmd) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return cmd->QueryInterface(NS_GET_IID(nsIReflowCommand), (void**)aInstancePtrResult);
}

#ifdef DEBUG_jesup
PRInt32 gReflows;
PRInt32 gReflowsInUse;
PRInt32 gReflowsInUseMax;
PRInt32 gReflowsZero;
PRInt32 gReflowsAuto;
PRInt32 gReflowsLarger;
PRInt32 gReflowsMaxZero;
PRInt32 gReflowsMaxAuto;
PRInt32 gReflowsMaxLarger;
class mPathStats {
public:
  mPathStats();
  ~mPathStats();
};
mPathStats::mPathStats()
{
}
mPathStats::~mPathStats()
{
  printf("nsHTMLReflowCommand->mPath stats:\n");
  printf("\tNumber created:   %d\n",gReflows);
  printf("\tNumber in-use:    %d\n",gReflowsInUseMax);

  printf("\tNumber size == 0: %d\n",gReflowsZero);
  printf("\tNumber size <= 8: %d\n",gReflowsAuto);
  printf("\tNumber size  > 8: %d\n",gReflowsLarger);
  printf("\tNum max size == 0: %d\n",gReflowsMaxZero);
  printf("\tNum max size <= 8: %d\n",gReflowsMaxAuto);
  printf("\tNum max size  > 8: %d\n",gReflowsMaxLarger);
}

// Just so constructor/destructor's get called
mPathStats gmPathStats;
#endif

// Construct a reflow command given a target frame, reflow command type,
// and optional child frame
nsHTMLReflowCommand::nsHTMLReflowCommand(nsIFrame*  aTargetFrame,
                                         ReflowType aReflowType,
                                         nsIFrame*  aChildFrame,
                                         nsIAtom*   aAttribute)
  : mType(aReflowType), mTargetFrame(aTargetFrame), mChildFrame(aChildFrame),
    mPrevSiblingFrame(nsnull),
    mAttribute(aAttribute),
    mListName(nsnull),
    mFlags(0)
{
  NS_PRECONDITION(mTargetFrame != nsnull, "null target frame");
  if (nsnull!=mAttribute)
    NS_ADDREF(mAttribute);
  NS_INIT_REFCNT();
#ifdef DEBUG_jesup
  gReflows++;
  gReflowsInUse++;
  if (gReflowsInUse > gReflowsInUseMax)
    gReflowsInUseMax = gReflowsInUse;
#endif
}

nsHTMLReflowCommand::~nsHTMLReflowCommand()
{
#ifdef DEBUG_jesup
  if (mPath.GetArraySize() == 0)
    gReflowsMaxZero++;
  else if (mPath.GetArraySize() <= 8)
    gReflowsMaxAuto++;
  else
    gReflowsMaxLarger++;
  gReflowsInUse--;
#endif

  NS_IF_RELEASE(mAttribute);
  NS_IF_RELEASE(mListName);
}

NS_IMPL_ISUPPORTS1(nsHTMLReflowCommand, nsIReflowCommand)

nsIFrame* nsHTMLReflowCommand::GetContainingBlock(nsIFrame* aFloater) const
{
  nsIFrame* containingBlock;
  aFloater->GetParent(&containingBlock);
  return containingBlock;
}

void nsHTMLReflowCommand::BuildPath()
{
#ifdef DEBUG_jesup
  if (mPath.Count() == 0)
    gReflowsZero++;
  else if (mPath.Count() <= 8)
    gReflowsAuto++;
  else
    gReflowsLarger++;
#endif

  mPath.Clear();

  // Floating frames are handled differently. The path goes from the target
  // frame to the containing block, and then up the hierarchy
  const nsStyleDisplay* display;
  mTargetFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
  if (NS_STYLE_FLOAT_NONE != display->mFloats) {
    mPath.AppendElement((void*)mTargetFrame);

    for (nsIFrame* f = GetContainingBlock(mTargetFrame); nsnull != f; f->GetParent(&f)) {
      mPath.AppendElement((void*)f);
    }

  } else {
    for (nsIFrame* f = mTargetFrame; nsnull != f; f->GetParent(&f)) {
      mPath.AppendElement((void*)f);
    }
  }
}

NS_IMETHODIMP nsHTMLReflowCommand::Dispatch(nsIPresContext*      aPresContext,
                                            nsHTMLReflowMetrics& aDesiredSize,
                                            const nsSize&        aMaxSize,
                                            nsIRenderingContext& aRendContext)
{
  // Build the path from the target frame (index 0) to the root frame
  BuildPath();

  // Send an incremental reflow notification to the root frame
  nsIFrame* root = (nsIFrame*)mPath[mPath.Count() - 1];

#ifdef NS_DEBUG
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  if (shell) {
    nsIFrame* rootFrame;
    shell->GetRootFrame(&rootFrame);
    NS_ASSERTION(rootFrame == root, "bad root frame");
  }
#endif

  if (nsnull != root) {    
    mPath.RemoveElementAt(mPath.Count() - 1);

    nsHTMLReflowState reflowState(aPresContext, root, *this,
                                  &aRendContext, aMaxSize);
    nsReflowStatus    status;

    root->WillReflow(aPresContext);
    nsContainerFrame::PositionFrameView(aPresContext, root);
    root->Reflow(aPresContext, aDesiredSize, reflowState, status);
    root->SizeTo(aPresContext, aDesiredSize.width, aDesiredSize.height);
    nsIView* view;
    root->GetView(aPresContext, &view);
    if (view) {
      nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, root, view,
                                                 nsnull);
    }
    root->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
  }

  return NS_OK;
}

NS_IMETHODIMP nsHTMLReflowCommand::GetNext(nsIFrame*& aNextFrame, PRBool aRemove)
{
  PRInt32 count = mPath.Count();

  aNextFrame = nsnull;
  if (count > 0) {
    aNextFrame = (nsIFrame*)mPath[count - 1];
    if (aRemove) {
      mPath.RemoveElementAt(count - 1);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsHTMLReflowCommand::GetTarget(nsIFrame*& aTargetFrame) const
{
  aTargetFrame = mTargetFrame;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLReflowCommand::SetTarget(nsIFrame* aTargetFrame)
{
  mTargetFrame = aTargetFrame;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLReflowCommand::GetType(ReflowType& aReflowType) const
{
  aReflowType = mType;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLReflowCommand::GetAttribute(nsIAtom *& aAttribute) const
{
  aAttribute = mAttribute;
  if (nsnull!=aAttribute)
    NS_ADDREF(aAttribute);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLReflowCommand::GetChildFrame(nsIFrame*& aChildFrame) const
{
  aChildFrame = mChildFrame;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLReflowCommand::GetChildListName(nsIAtom*& aListName) const
{
  aListName = mListName;
  NS_IF_ADDREF(aListName);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLReflowCommand::SetChildListName(nsIAtom* aListName)
{
  mListName = aListName;
  NS_IF_ADDREF(mListName);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLReflowCommand::GetPrevSiblingFrame(nsIFrame*& aSiblingFrame) const
{
  aSiblingFrame = mPrevSiblingFrame;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLReflowCommand::List(FILE* out) const
{
#ifdef DEBUG
  static const char* kReflowCommandType[] = {
    "ContentChanged",
    "StyleChanged",
    "ReflowDirty",
    "UserDefined",
  };

  fprintf(out, "ReflowCommand@%p[%s]:",
          this, kReflowCommandType[mType]);
  if (mTargetFrame) {
    fprintf(out, " target=");
    nsFrame::ListTag(out, mTargetFrame);
  }
  if (mChildFrame) {
    fprintf(out, " child=");
    nsFrame::ListTag(out, mChildFrame);
  }
  if (mPrevSiblingFrame) {
    fprintf(out, " prevSibling=");
    nsFrame::ListTag(out, mPrevSiblingFrame);
  }
  if (mAttribute) {
    fprintf(out, " attr=");
    nsAutoString attr;
    mAttribute->ToString(attr);
    fputs(attr, out);
  }
  if (mListName) {
    fprintf(out, " list=");
    nsAutoString attr;
    mListName->ToString(attr);
    fputs(attr, out);
  }
  fprintf(out, "\n");

  // Show the path, but without using mPath which is in an undefined
  // state at this point.
  if (mTargetFrame) {
    // Floating frames are handled differently. The path goes from the target
    // frame to the containing block, and then up the hierarchy
    PRBool didOne = PR_FALSE;
    nsIFrame* start = mTargetFrame;
    const nsStyleDisplay* display;
    mTargetFrame->GetStyleData(eStyleStruct_Display,
                               (const nsStyleStruct*&) display);
    if (NS_STYLE_FLOAT_NONE != display->mFloats) {
      start = GetContainingBlock(mTargetFrame);
    }
    for (nsIFrame* f = start; nsnull != f; f->GetParent(&f)) {
      if (f != mTargetFrame) {
        fprintf(out, " ");
        nsFrame::ListTag(out, f);
        didOne = PR_TRUE;
      }
    }
    if (didOne) {
      fprintf(out, "\n");
    }
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLReflowCommand::GetFlags(PRInt32* aFlags)
{
  *aFlags = mFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLReflowCommand::SetFlags(PRInt32 aFlags)
{
  mFlags = aFlags;
  return NS_OK;
}
