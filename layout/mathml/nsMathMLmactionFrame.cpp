/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"

#include "nsCSSRendering.h"
#include "prprf.h"         // For PR_snprintf()

#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIWebBrowserChrome.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMElement.h"

#include "nsIDOMEventTarget.h"

#include "nsMathMLmactionFrame.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"
#include "nsDisplayList.h"
#include "nsContentUtils.h"

//
// <maction> -- bind actions to a subexpression - implementation
//

#define NS_MATHML_ACTION_TYPE_NONE         0
#define NS_MATHML_ACTION_TYPE_TOGGLE       1
#define NS_MATHML_ACTION_TYPE_STATUSLINE   2
#define NS_MATHML_ACTION_TYPE_TOOLTIP      3 // unsupported
#define NS_MATHML_ACTION_TYPE_RESTYLE      4


nsIFrame*
NS_NewMathMLmactionFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmactionFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmactionFrame)

nsMathMLmactionFrame::~nsMathMLmactionFrame()
{
  // unregister us as a mouse event listener ...
  //  printf("maction:%p unregistering as mouse event listener ...\n", this);
  if (mListener) {
    mContent->RemoveEventListener(NS_LITERAL_STRING("click"), mListener,
                                  PR_FALSE);
    mContent->RemoveEventListener(NS_LITERAL_STRING("mouseover"), mListener,
                                  PR_FALSE);
    mContent->RemoveEventListener(NS_LITERAL_STRING("mouseout"), mListener,
                                  PR_FALSE);
  }
}

NS_IMETHODIMP
nsMathMLmactionFrame::Init(nsIContent*      aContent,
                           nsIFrame*        aParent,
                           nsIFrame*        aPrevInFlow)
{
  nsAutoString value, prefix;

  // Init our local attributes

  mChildCount = -1; // these will be updated in GetSelectedFrame()
  mSelection = 0;
  mSelectedFrame = nsnull;
  nsRefPtr<nsStyleContext> newStyleContext;

  mActionType = NS_MATHML_ACTION_TYPE_NONE;
  aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::actiontype_, value);
  if (!value.IsEmpty()) {
    if (value.EqualsLiteral("toggle"))
      mActionType = NS_MATHML_ACTION_TYPE_TOGGLE;

    // XXX use goto to jump out of these if?

    if (NS_MATHML_ACTION_TYPE_NONE == mActionType) {
      // expected tooltip prefix (8ch)...
      if (8 < value.Length() && 0 == value.Find("tooltip#"))
        mActionType = NS_MATHML_ACTION_TYPE_TOOLTIP;
    }

    if (NS_MATHML_ACTION_TYPE_NONE == mActionType) {
      // expected statusline prefix (11ch)...
      if (11 < value.Length() && 0 == value.Find("statusline#"))
        mActionType = NS_MATHML_ACTION_TYPE_STATUSLINE;
    }

    if (NS_MATHML_ACTION_TYPE_NONE == mActionType) {
      // expected restyle prefix (8ch)...
      if (8 < value.Length() && 0 == value.Find("restyle#")) {
        mActionType = NS_MATHML_ACTION_TYPE_RESTYLE;
        mRestyle = value;

        // Here is the situation:
        // When the attribute [actiontype="restyle#id"] is set, the Style System has
        // given us the associated style. But we want to start with our default style.

        // So... first, remove the attribute actiontype="restyle#id"
        // XXXbz this is pretty messed up, since this can change whether we
        // should have a frame at all.  This really needs a better solution.
        PRBool notify = PR_FALSE; // don't trigger a reflow yet!
        aContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::actiontype_, notify);

        // then, re-resolve our style
        nsStyleContext* parentStyleContext = GetStyleContext()->GetParent();
        newStyleContext = PresContext()->StyleSet()->
          ResolveStyleFor(aContent->AsElement(), parentStyleContext);

        if (!newStyleContext) 
          mRestyle.Truncate();
        else {
          if (newStyleContext != GetStyleContext())
            SetStyleContextWithoutNotification(newStyleContext);
          else
            mRestyle.Truncate();
        }
      }
    }
  }

  // Let the base class do the rest
  return nsMathMLContainerFrame::Init(aContent, aParent, aPrevInFlow);
}

NS_IMETHODIMP
nsMathMLmactionFrame::TransmitAutomaticData() {
  // The REC defines the following element to be space-like:
  // * an maction element whose selected sub-expression exists and is
  //   space-like;
  nsIMathMLFrame* mathMLFrame = do_QueryFrame(mSelectedFrame);
  if (mathMLFrame && mathMLFrame->IsSpaceLike()) {
    mPresentationData.flags |= NS_MATHML_SPACE_LIKE;
  } else {
    mPresentationData.flags &= ~NS_MATHML_SPACE_LIKE;
  }

  // The REC defines the following element to be an embellished operator:
  // * an maction element whose selected sub-expression exists and is an
  //   embellished operator;
  mPresentationData.baseFrame = mSelectedFrame;
  GetEmbellishDataFrom(mSelectedFrame, mEmbellishData);

  return NS_OK;
}

nsresult
nsMathMLmactionFrame::ChildListChanged(PRInt32 aModType)
{
  // update cached values
  mChildCount = -1;
  mSelection = 0;
  mSelectedFrame = nsnull;
  GetSelectedFrame();

  return nsMathMLContainerFrame::ChildListChanged(aModType);
}

// return the frame whose number is given by the attribute selection="number"
nsIFrame* 
nsMathMLmactionFrame::GetSelectedFrame()
{
  nsAutoString value;
  PRInt32 selection; 

  GetAttribute(mContent, mPresentationData.mstyle, nsGkAtoms::selection_,
               value);
  if (!value.IsEmpty()) {
    PRInt32 errorCode;
    selection = value.ToInteger(&errorCode);
    if (NS_FAILED(errorCode)) 
      selection = 1;
  }
  else selection = 1; // default is first frame

  if (-1 != mChildCount) { // we have been in this function before...
    // cater for invalid user-supplied selection
    if (selection > mChildCount || selection < 1) 
      selection = 1;
    // quick return if it is identical with our cache
    if (selection == mSelection) 
      return mSelectedFrame;
  }

  // get the selected child and cache new values...
  PRInt32 count = 0;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (!mSelectedFrame) 
      mSelectedFrame = childFrame; // default is first child
    if (++count == selection) 
      mSelectedFrame = childFrame;

    childFrame = childFrame->GetNextSibling();
  }
  // cater for invalid user-supplied selection
  if (selection > count || selection < 1) 
    selection = 1;

  mChildCount = count;
  mSelection = selection;

  return mSelectedFrame;
}

NS_IMETHODIMP
nsMathMLmactionFrame::SetInitialChildList(ChildListID     aListID,
                                          nsFrameList&    aChildList)
{
  nsresult rv = nsMathMLContainerFrame::SetInitialChildList(aListID, aChildList);

  // This very first call to GetSelectedFrame() will cause us to be marked as an
  // embellished operator if the selected child is an embellished operator
  if (!GetSelectedFrame()) {
    mActionType = NS_MATHML_ACTION_TYPE_NONE;
  }
  else {
    // create mouse event listener and register it
    mListener = new nsMathMLmactionFrame::MouseListener(this);
    // printf("maction:%p registering as mouse event listener ...\n", this);
    mContent->AddEventListener(NS_LITERAL_STRING("click"), mListener,
                               PR_FALSE, PR_FALSE);
    mContent->AddEventListener(NS_LITERAL_STRING("mouseover"), mListener,
                               PR_FALSE, PR_FALSE);
    mContent->AddEventListener(NS_LITERAL_STRING("mouseout"), mListener,
                               PR_FALSE, PR_FALSE);
  }
  return rv;
}

//  Only paint the selected child...
NS_IMETHODIMP
nsMathMLmactionFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                       const nsRect&           aDirtyRect,
                                       const nsDisplayListSet& aLists)
{
  nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIFrame* childFrame = GetSelectedFrame();
  if (childFrame) {
    // Put the child's background directly onto the content list
    nsDisplayListSet set(aLists, aLists.Content());
    // The children should be in content order
    rv = BuildDisplayListForChild(aBuilder, childFrame, aDirtyRect, set);
    NS_ENSURE_SUCCESS(rv, rv);
  }

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  // visual debug
  rv = DisplayBoundingMetrics(aBuilder, this, mReference, mBoundingMetrics, aLists);
#endif
  return rv;
}

// Only reflow the selected child ...
NS_IMETHODIMP
nsMathMLmactionFrame::Reflow(nsPresContext*          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;
  aStatus = NS_FRAME_COMPLETE;
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  mBoundingMetrics = nsBoundingMetrics();
  nsIFrame* childFrame = GetSelectedFrame();
  if (childFrame) {
    nsSize availSize(aReflowState.ComputedWidth(), NS_UNCONSTRAINEDSIZE);
    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                       childFrame, availSize);
    rv = ReflowChild(childFrame, aPresContext, aDesiredSize,
                     childReflowState, aStatus);
    SaveReflowAndBoundingMetricsFor(childFrame, aDesiredSize,
                                    aDesiredSize.mBoundingMetrics);
    mBoundingMetrics = aDesiredSize.mBoundingMetrics;
  }
  FinalizeReflow(*aReflowState.rendContext, aDesiredSize);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;
}

// Only place the selected child ...
/* virtual */ nsresult
nsMathMLmactionFrame::Place(nsRenderingContext& aRenderingContext,
                            PRBool               aPlaceOrigin,
                            nsHTMLReflowMetrics& aDesiredSize)
{
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  mBoundingMetrics = nsBoundingMetrics();
  nsIFrame* childFrame = GetSelectedFrame();
  if (childFrame) {
    GetReflowAndBoundingMetricsFor(childFrame, aDesiredSize, mBoundingMetrics);
    if (aPlaceOrigin) {
      FinishReflowChild(childFrame, PresContext(), nsnull, aDesiredSize, 0, 0, 0);
    }
    mReference.x = 0;
    mReference.y = aDesiredSize.ascent;
  }
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  return NS_OK;
}

// ################################################################
// Event handlers 
// ################################################################

NS_IMPL_ISUPPORTS1(nsMathMLmactionFrame::MouseListener,
                   nsIDOMEventListener)


// helper to show a msg on the status bar
// curled from nsObjectFrame.cpp ...
void
ShowStatus(nsPresContext* aPresContext, nsString& aStatusMsg)
{
  nsCOMPtr<nsISupports> cont = aPresContext->GetContainer();
  if (cont) {
    nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(cont));
    if (docShellItem) {
      nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
      docShellItem->GetTreeOwner(getter_AddRefs(treeOwner));
      if (treeOwner) {
        nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(treeOwner));
        if (browserChrome) {
          browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_LINK, aStatusMsg.get());
        }
      }
    }
  }
}

NS_IMETHODIMP
nsMathMLmactionFrame::MouseListener::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("mouseover")) {
    mOwner->MouseOver();
  }
  else if (eventType.EqualsLiteral("click")) {
    mOwner->MouseClick();
  }
  else if (eventType.EqualsLiteral("mouseout")) {
    mOwner->MouseOut();
  }
  else {
    NS_ABORT();
  }

  return NS_OK;
}

void
nsMathMLmactionFrame::MouseOver()
{
  // see if we should display a status message
  if (NS_MATHML_ACTION_TYPE_STATUSLINE == mActionType) {
    nsAutoString value;
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::actiontype_, value);
    // expected statusline prefix (11ch)...
    if (11 < value.Length() && 0 == value.Find("statusline#")) {
      value.Cut(0, 11);
      ShowStatus(PresContext(), value);
    }
  }
}

void
nsMathMLmactionFrame::MouseOut()
{
  // see if we should remove the status message
  if (NS_MATHML_ACTION_TYPE_STATUSLINE == mActionType) {
    nsAutoString value;
    value.SetLength(0);
    ShowStatus(PresContext(), value);
  }
}

void
nsMathMLmactionFrame::MouseClick()
{
  if (NS_MATHML_ACTION_TYPE_TOGGLE == mActionType) {
    if (mChildCount > 1) {
      PRInt32 selection = (mSelection == mChildCount)? 1 : mSelection + 1;
      nsAutoString value;
      char cbuf[10];
      PR_snprintf(cbuf, sizeof(cbuf), "%d", selection);
      value.AssignASCII(cbuf);
      PRBool notify = PR_FALSE; // don't yet notify the document
      mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::selection_, value, notify);

      // Now trigger a content-changed reflow...
      PresContext()->PresShell()->
        FrameNeedsReflow(mSelectedFrame, nsIPresShell::eTreeChange,
                         NS_FRAME_IS_DIRTY);
    }
  }
  else if (NS_MATHML_ACTION_TYPE_RESTYLE == mActionType) {
    if (!mRestyle.IsEmpty()) {
      nsCOMPtr<nsIDOMElement> node( do_QueryInterface(mContent) );
      if (node.get()) {
        if (nsContentUtils::HasNonEmptyAttr(mContent, kNameSpaceID_None,
                                            nsGkAtoms::actiontype_))
          node->RemoveAttribute(NS_LITERAL_STRING("actiontype"));
        else
          node->SetAttribute(NS_LITERAL_STRING("actiontype"), mRestyle);

        // Trigger a style change reflow
        PresContext()->PresShell()->
          FrameNeedsReflow(mSelectedFrame, nsIPresShell::eStyleChange,
                           NS_FRAME_IS_DIRTY);
      }
    }
  }
}
