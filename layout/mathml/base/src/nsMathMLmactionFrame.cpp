/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is The University Of
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 */

#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsFrame.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleUtil.h"

#include "nsCSSRendering.h"
#include "prprf.h"         // For PR_snprintf()

#include "nsIWebShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIWebBrowserChrome.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDOMElement.h"

#include "nsIDOMEventReceiver.h"
#include "nsIDOMMouseListener.h"

#include "nsMathMLmactionFrame.h"

//
// <maction> -- bind actions to a subexpression - implementation
//

#define NS_MATHML_ACTION_TYPE_NONE         0
#define NS_MATHML_ACTION_TYPE_TOGGLE       1
#define NS_MATHML_ACTION_TYPE_STATUSLINE   2
#define NS_MATHML_ACTION_TYPE_TOOLTIP      3 // unsupported
#define NS_MATHML_ACTION_TYPE_RESTYLE      4

NS_INTERFACE_MAP_BEGIN(nsMathMLmactionFrame)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
NS_INTERFACE_MAP_END_INHERITING(nsMathMLContainerFrame)

NS_IMETHODIMP_(nsrefcnt) 
nsMathMLmactionFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsMathMLmactionFrame::Release(void)
{
  return NS_OK;
}

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
                           nsIStyleContext* aContext,
                           nsIFrame*        aPrevInFlow)
{
  nsAutoString value, prefix;

  // Init our local attributes

  mPresContext = aPresContext;
  
  mChildCount = -1; // these will be updated in GetSelectedFrame()
  mSelection = 0;
  mSelectedFrame = nsnull;

  mActionType = NS_MATHML_ACTION_TYPE_NONE;
  if (NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttribute(kNameSpaceID_None, 
                   nsMathMLAtoms::actiontype_, value)) {
    if (value.EqualsWithConversion("toggle"))
      mActionType = NS_MATHML_ACTION_TYPE_TOGGLE;

    // XXX use goto to jump out of these if?

    if (NS_MATHML_ACTION_TYPE_NONE == mActionType) {
      // expected tooltip prefix (8ch)...
      prefix.AssignWithConversion("tooltip#");
      if (8 < value.Length() && 0 == value.Find(prefix))
        mActionType = NS_MATHML_ACTION_TYPE_TOOLTIP;
    }

    if (NS_MATHML_ACTION_TYPE_NONE == mActionType) {
      // expected statusline prefix (11ch)...
      prefix.AssignWithConversion("statusline#");
      if (11 < value.Length() && 0 == value.Find(prefix))
        mActionType = NS_MATHML_ACTION_TYPE_STATUSLINE;
    }

    if (NS_MATHML_ACTION_TYPE_NONE == mActionType) {
      // expected restyle prefix (8ch)...
      prefix.AssignWithConversion("restyle#");
      if (8 < value.Length() && 0 == value.Find(prefix)) {
        mActionType = NS_MATHML_ACTION_TYPE_RESTYLE;
        mRestyle = value;

        // Here is the situation:
        // When the attribute [actiontype="restyle#id"] is set, the Style System has
        // given us the associated style. But we want to start with our default style.

        // So... first, remove the attribute actiontype="restyle#id"
        value.SetLength(0);
        PRBool notify = PR_FALSE; // don't trigger a reflow yet!
        aContent->SetAttribute(kNameSpaceID_None, nsMathMLAtoms::actiontype_, value, notify);

        // then, re-resolve our style
        nsCOMPtr<nsIStyleContext> parentStyleContext;
        aParent->GetStyleContext(getter_AddRefs(parentStyleContext));
        nsIStyleContext* newStyleContext;
        aPresContext->ResolveStyleContextFor(aContent, parentStyleContext,
                                             PR_FALSE, &newStyleContext);
        if (!newStyleContext) 
          mRestyle.Truncate();
        else {
          if (newStyleContext != aContext)
            aContext = newStyleContext;
          else {
            NS_RELEASE(newStyleContext);
            mRestyle.Truncate();
          }
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
  nsresult rv = NS_OK;
  nsAutoString value;
  PRInt32 selection; 

  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
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
    if (!IsOnlyWhitespace(childFrame)) {   
      if (!mSelectedFrame) 
        mSelectedFrame = childFrame; // default is first child
      if (++count == selection) 
        mSelectedFrame = childFrame;
    }
    childFrame->GetNextSibling(&childFrame);
  }
  // cater for invalid user-supplied selection
  if (selection > count || selection < 1) 
    selection = 1;

  mChildCount = count;
  mSelection = selection;

  // if the selected child is an embellished operator,
  // we become embellished as well
  mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_OPERATOR;
  mEmbellishData.firstChild = nsnull;
  mEmbellishData.core = nsnull;
  mEmbellishData.direction = NS_STRETCH_DIRECTION_UNSUPPORTED;
  if (mSelectedFrame) {
    nsIMathMLFrame* aMathMLFrame = nsnull;
    rv = mSelectedFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
    if (NS_SUCCEEDED(rv) && aMathMLFrame) {
      nsEmbellishData embellishData;
      aMathMLFrame->GetEmbellishData(embellishData);
      if (NS_MATHML_IS_EMBELLISH_OPERATOR(embellishData.flags) && embellishData.firstChild) {
        mEmbellishData.flags |= NS_MATHML_EMBELLISH_OPERATOR;
        mEmbellishData.firstChild = mSelectedFrame; // yes!
        mEmbellishData.core = embellishData.core;
        mEmbellishData.direction = embellishData.direction;
      }
    }
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
  // embellished operator if the selected child is an embellished operator,
  GetSelectedFrame();

  // register us as a mouse event listener ...
//  printf("maction:%p registering as mouse event listener ...\n", this);
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(mContent));
  receiver->AddEventListenerByIID(this, NS_GET_IID(nsIDOMMouseListener));

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
    nsPoint pt;
    pt.MoveTo(aPoint.x - mRect.x, aPoint.y - mRect.y);
    return childFrame->GetFrameForPoint(aPresContext, pt, aWhichLayer, aFrame);
  }
  else
    return nsFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
}

//  Only paint the selected child...
NS_IMETHODIMP
nsMathMLmactionFrame::Paint(nsIPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect&        aDirtyRect,
                            nsFramePaintLayer    aWhichLayer)
{
  const nsStyleDisplay* disp = 
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    if (disp->IsVisible() && mRect.width && mRect.height) {
      // Paint our background and border
      PRIntn skipSides = GetSkipSides();
      const nsStyleColor* color = (const nsStyleColor*)
        mStyleContext->GetStyleData(eStyleStruct_Color);
      const nsStyleSpacing* spacing = (const nsStyleSpacing*)
        mStyleContext->GetStyleData(eStyleStruct_Spacing);

      nsRect  rect(0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *color, *spacing, 0, 0);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *spacing, mStyleContext, skipSides);
      nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
                                   aDirtyRect, rect, *spacing, mStyleContext, 0);
    }
  }

  nsIFrame* childFrame = GetSelectedFrame();
  if (childFrame)
    PaintChild(aPresContext, aRenderingContext, aDirtyRect, childFrame, aWhichLayer);

  return NS_OK;
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
      FinishReflowChild(childFrame, aPresContext, aDesiredSize, 0, 0, 0);
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
  nsCOMPtr<nsISupports> cont;
  nsresult rv = aPresContext->GetContainer(getter_AddRefs(cont));
  if (NS_SUCCEEDED(rv) && cont) {
    nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(cont));
    if (docShellItem) {

      nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
      docShellItem->GetTreeOwner(getter_AddRefs(treeOwner));

      if(treeOwner)
      {
      nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(treeOwner));

      if(browserChrome)
        browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_SCRIPT, aStatusMsg.GetUnicode());
      }
    }
  }
  return rv;
}

nsresult
nsMathMLmactionFrame::MouseOver(nsIDOMEvent* aMouseEvent) 
{
  // see if we should display a status message
  if (NS_MATHML_ACTION_TYPE_STATUSLINE == mActionType) 
  {
    nsAutoString value;
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                     nsMathMLAtoms::actiontype_, value)) 
    {
      // expected statusline prefix (11ch)...
      nsAutoString statusline;
      statusline.AssignWithConversion("statusline#");
      if (11 < value.Length() && 0 == value.Find(statusline)) {
        value.Cut(0, 11);
        ShowStatus(mPresContext, value);
      }
    }
  }
  return NS_OK;
}

nsresult
nsMathMLmactionFrame::MouseOut(nsIDOMEvent* aMouseEvent) 
{ 
  // see if we should remove the status message
  if (NS_MATHML_ACTION_TYPE_STATUSLINE == mActionType) 
  {
    nsAutoString value;
    value.SetLength(0);
    ShowStatus(mPresContext, value);
  }
  return NS_OK;
}

nsresult
nsMathMLmactionFrame::MouseClick(nsIDOMEvent* aMouseEvent)
{
  nsAutoString value;
  if (NS_MATHML_ACTION_TYPE_TOGGLE == mActionType) 
  {
    if (mChildCount > 1) {
      PRInt32 selection = (mSelection == mChildCount)? 1 : mSelection + 1;
      char cbuf[10];
      PR_snprintf(cbuf, sizeof(cbuf), "%d", selection);
      value.AssignWithConversion(cbuf);
      PRBool notify = PR_FALSE; // don't yet notify the document
      mContent->SetAttribute(kNameSpaceID_None, nsMathMLAtoms::selection_, value, notify);

      // Now trigger a content-changed reflow...
//      ContentChanged(mPresContext, mContent, nsnull);

      nsCOMPtr<nsIPresShell> presShell;
      mPresContext->GetShell(getter_AddRefs(presShell));
      ReflowDirtyChild(presShell, mSelectedFrame);
    }
  }
  else if (NS_MATHML_ACTION_TYPE_RESTYLE == mActionType) 
  {
    if (0 < mRestyle.Length()) {
      nsCOMPtr<nsIDOMElement> node( do_QueryInterface(mContent) );
      if (node.get()) {
        if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                         nsMathMLAtoms::actiontype_, value))
          node->RemoveAttribute(NS_ConvertASCIItoUCS2("actiontype"));
        else
          node->SetAttribute(NS_ConvertASCIItoUCS2("actiontype"), mRestyle);
      }
    }
  }
  return NS_OK;
}
