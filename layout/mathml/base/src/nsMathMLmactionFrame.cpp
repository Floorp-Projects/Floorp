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
#include "nsIPresContext.h"
#include "nsUnitConversion.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"

#include "nsCSSRendering.h"
#include "prprf.h"         // For PR_snprintf()

#include "nsIWebShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIWebBrowserChrome.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMElement.h"

#include "nsIDOMEventReceiver.h"
#include "nsIDOMMouseListener.h"

#include "nsMathMLmactionFrame.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"

//
// <maction> -- bind actions to a subexpression - implementation
//

#define NS_MATHML_ACTION_TYPE_NONE         0
#define NS_MATHML_ACTION_TYPE_TOGGLE       1
#define NS_MATHML_ACTION_TYPE_STATUSLINE   2
#define NS_MATHML_ACTION_TYPE_TOOLTIP      3 // unsupported
#define NS_MATHML_ACTION_TYPE_RESTYLE      4

NS_IMPL_ADDREF_INHERITED(nsMathMLmactionFrame, nsMathMLContainerFrame)
NS_IMPL_RELEASE_INHERITED(nsMathMLmactionFrame, nsMathMLContainerFrame)
NS_IMPL_QUERY_INTERFACE_INHERITED1(nsMathMLmactionFrame, nsMathMLContainerFrame, nsIDOMMouseListener)

nsresult
NS_NewMathMLmactionFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmactionFrame* it = new (aPresShell) nsMathMLmactionFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmactionFrame::nsMathMLmactionFrame()
{
}

nsMathMLmactionFrame::~nsMathMLmactionFrame()
{
  // unregister us as a mouse event listener ...
//  printf("maction:%p unregistering as mouse event listener ...\n", this);
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(mContent));
  receiver->RemoveEventListenerByIID(this, NS_GET_IID(nsIDOMMouseListener));
}

NS_IMETHODIMP
nsMathMLmactionFrame::Init(nsIPresContext*  aPresContext,
                           nsIContent*      aContent,
                           nsIFrame*        aParent,
                           nsStyleContext*  aContext,
                           nsIFrame*        aPrevInFlow)
{
  nsAutoString value, prefix;

  // Init our local attributes

  mPresContext = aPresContext;
 
  mWasRestyled = PR_FALSE;
  mChildCount = -1; // these will be updated in GetSelectedFrame()
  mSelection = 0;
  mSelectedFrame = nsnull;
  nsRefPtr<nsStyleContext> newStyleContext;

  mActionType = NS_MATHML_ACTION_TYPE_NONE;
  if (NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttr(kNameSpaceID_None, 
                   nsMathMLAtoms::actiontype_, value)) {
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
        PRBool notify = PR_FALSE; // don't trigger a reflow yet!
        aContent->UnsetAttr(kNameSpaceID_None, nsMathMLAtoms::actiontype_, notify);

        // then, re-resolve our style
        nsStyleContext* parentStyleContext = aParent->GetStyleContext();
        newStyleContext = aPresContext->StyleSet()->
          ResolveStyleFor(aContent, parentStyleContext);

        if (!newStyleContext) 
          mRestyle.Truncate();
        else {
          if (newStyleContext != aContext)
            aContext = newStyleContext;
          else
            mRestyle.Truncate();
        }
      }
    }
  }

  // Let the base class do the rest
  return nsMathMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
}

// return the frame whose number is given by the attribute selection="number"
nsIFrame* 
nsMathMLmactionFrame::GetSelectedFrame()
{
  nsAutoString value;
  PRInt32 selection; 

  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None, 
                   nsMathMLAtoms::selection_, value)) {
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

  // if the selected child is an embellished operator,
  // we become embellished as well
  GetEmbellishDataFrom(mSelectedFrame, mEmbellishData);
  if (NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags)) {
    mEmbellishData.nextFrame = mSelectedFrame; // yes!
  }

  return mSelectedFrame;
}

NS_IMETHODIMP
nsMathMLmactionFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                          nsIAtom*        aListName,
                                          nsIFrame*       aChildList)
{
  nsresult rv = nsMathMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  // This very first call to GetSelectedFrame() will cause us to be marked as an
  // embellished operator if the selected child is an embellished operator
  if (!GetSelectedFrame()) {
    mActionType = NS_MATHML_ACTION_TYPE_NONE;
  }
  else {
    // register us as a mouse event listener ...
    // printf("maction:%p registering as mouse event listener ...\n", this);
    nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(mContent));
    receiver->AddEventListenerByIID(this, NS_GET_IID(nsIDOMMouseListener));
  }
  return rv;
}

// Return the selected frame ...
NS_IMETHODIMP
nsMathMLmactionFrame::GetFrameForPoint(nsIPresContext*   aPresContext,
                                       const nsPoint&    aPoint,
                                       nsFramePaintLayer aWhichLayer,
                                       nsIFrame**        aFrame)
{
  nsIFrame* childFrame = GetSelectedFrame();
  if (childFrame) {
    nsPoint pt(aPoint.x - mRect.x, aPoint.y - mRect.y);
    return childFrame->GetFrameForPoint(aPresContext, pt, aWhichLayer, aFrame);
  }
  return nsFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
}

//  Only paint the selected child...
NS_IMETHODIMP
nsMathMLmactionFrame::Paint(nsIPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect&        aDirtyRect,
                            nsFramePaintLayer    aWhichLayer,
                            PRUint32             aFlags)
{
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    PaintSelf(aPresContext, aRenderingContext, aDirtyRect);
  }

  nsIFrame* childFrame = GetSelectedFrame();
  if (childFrame)
    PaintChild(aPresContext, aRenderingContext, aDirtyRect, childFrame, aWhichLayer);

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  // visual debug
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer &&
      NS_MATHML_PAINT_BOUNDING_METRICS(mPresentationData.flags)) {
    aRenderingContext.SetColor(NS_RGB(0,0,255));

    nscoord x = mReference.x + mBoundingMetrics.leftBearing;
    nscoord y = mReference.y - mBoundingMetrics.ascent;
    nscoord w = mBoundingMetrics.rightBearing - mBoundingMetrics.leftBearing;
    nscoord h = mBoundingMetrics.ascent + mBoundingMetrics.descent;

    aRenderingContext.DrawRect(x,y,w,h);
  }
#endif
  return NS_OK;
}

// Only reflow the selected child ...
NS_IMETHODIMP
nsMathMLmactionFrame::Reflow(nsIPresContext*          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = aDesiredSize.descent = 0;
  mBoundingMetrics.Clear();
  nsIFrame* childFrame = GetSelectedFrame();
  if (childFrame) {
    nsReflowReason reason = aReflowState.reason;
    if (mWasRestyled) {
      mWasRestyled = PR_FALSE;
      // If we have just been restyled, make sure to reflow our
      // selected child with a StyleChange reflow reason so that
      // it doesn't over-optimize its reflow. In principle we shouldn't
      // need to do this because we posted a style changed reflow (see
      // MouseClick() below). But that reason can be (and usually, it is)
      // changed in the reflow chain and we don't have much control other
      // than making sure that the right value is reset here.
      reason = eReflowReason_StyleChange;
    }

    nsSize availSize(aReflowState.mComputedWidth, aReflowState.mComputedHeight);
    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                       childFrame, availSize, reason);
    rv = ReflowChild(childFrame, aPresContext, aDesiredSize,
                     childReflowState, aStatus);
    childFrame->SetRect(nsRect(aDesiredSize.descent,aDesiredSize.ascent,
                        aDesiredSize.width,aDesiredSize.height));
    mBoundingMetrics = aDesiredSize.mBoundingMetrics;
    FinalizeReflow(aPresContext, *aReflowState.rendContext, aDesiredSize);
  }
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;
}

// Only place the selected child ...
NS_IMETHODIMP
nsMathMLmactionFrame::Place(nsIPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            PRBool               aPlaceOrigin,
                            nsHTMLReflowMetrics& aDesiredSize)
{
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = aDesiredSize.descent = 0;
  mBoundingMetrics.Clear();
  nsIFrame* childFrame = GetSelectedFrame();
  if (childFrame) {
    GetReflowAndBoundingMetricsFor(childFrame, aDesiredSize, mBoundingMetrics);
    if (aPlaceOrigin) {
      FinishReflowChild(childFrame, aPresContext, nsnull, aDesiredSize, 0, 0, 0);
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

// helper to show a msg on the status bar
// curled from nsObjectFrame.cpp ...
nsresult
nsMathMLmactionFrame::ShowStatus(nsIPresContext* aPresContext,
                                 nsString&       aStatusMsg)
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
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmactionFrame::MouseOver(nsIDOMEvent* aMouseEvent) 
{
  // see if we should display a status message
  if (NS_MATHML_ACTION_TYPE_STATUSLINE == mActionType) {
    nsAutoString value;
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None, 
                     nsMathMLAtoms::actiontype_, value)) {
      // expected statusline prefix (11ch)...
      if (11 < value.Length() && 0 == value.Find("statusline#")) {
        value.Cut(0, 11);
        ShowStatus(mPresContext, value);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmactionFrame::MouseOut(nsIDOMEvent* aMouseEvent) 
{ 
  // see if we should remove the status message
  if (NS_MATHML_ACTION_TYPE_STATUSLINE == mActionType) {
    nsAutoString value;
    value.SetLength(0);
    ShowStatus(mPresContext, value);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmactionFrame::MouseClick(nsIDOMEvent* aMouseEvent)
{
  nsAutoString value;
  if (NS_MATHML_ACTION_TYPE_TOGGLE == mActionType) {
    if (mChildCount > 1) {
      PRInt32 selection = (mSelection == mChildCount)? 1 : mSelection + 1;
      char cbuf[10];
      PR_snprintf(cbuf, sizeof(cbuf), "%d", selection);
      value.AssignASCII(cbuf);
      PRBool notify = PR_FALSE; // don't yet notify the document
      mContent->SetAttr(kNameSpaceID_None, nsMathMLAtoms::selection_, value, notify);

      // Now trigger a content-changed reflow...
      ReflowDirtyChild(mPresContext->PresShell(), mSelectedFrame);
    }
  }
  else if (NS_MATHML_ACTION_TYPE_RESTYLE == mActionType) {
    if (!mRestyle.IsEmpty()) {
      nsCOMPtr<nsIDOMElement> node( do_QueryInterface(mContent) );
      if (node.get()) {
        if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None, 
                         nsMathMLAtoms::actiontype_, value))
          node->RemoveAttribute(NS_LITERAL_STRING("actiontype"));
        else
          node->SetAttribute(NS_LITERAL_STRING("actiontype"), mRestyle);

        // At this stage, our style sub-tree has been re-resolved
        mWasRestyled = PR_TRUE;

        // Cancel the reflow command that the change of attribute has
        // caused, and post a style changed reflow request that is instead
        // targeted at our selected frame
        nsIPresShell *presShell = mPresContext->PresShell();
        presShell->CancelReflowCommand(this, nsnull);
        nsFrame::CreateAndPostReflowCommand(presShell, mSelectedFrame, 
          eReflowType_StyleChanged, nsnull, nsnull, nsnull);
      }
    }
  }
  return NS_OK;
}
