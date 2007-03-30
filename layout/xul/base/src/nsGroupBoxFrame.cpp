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

// YY need to pass isMultiple before create called

#include "nsBoxFrame.h"
#include "nsCSSRendering.h"
#include "nsStyleContext.h"
#include "nsDisplayList.h"

class nsGroupBoxFrame : public nsBoxFrame {
public:

  nsGroupBoxFrame(nsIPresShell* aShell, nsStyleContext* aContext):
    nsBoxFrame(aShell, aContext) {}

  NS_IMETHOD GetBorderAndPadding(nsMargin& aBorderAndPadding);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const {
    return MakeFrameName(NS_LITERAL_STRING("GroupBoxFrame"), aResult);
  }
#endif

  void PaintBorderBackground(nsIRenderingContext& aRenderingContext,
      nsPoint aPt, const nsRect& aDirtyRect);

  // make sure we our kids get our orient and align instead of us.
  // our child box has no content node so it will search for a parent with one.
  // that will be us.
  virtual void GetInitialOrientation(PRBool& aHorizontal) { aHorizontal = PR_FALSE; }
  virtual PRBool GetInitialHAlignment(Halignment& aHalign)  { aHalign = hAlign_Left; return PR_TRUE; } 
  virtual PRBool GetInitialVAlignment(Valignment& aValign)  { aValign = vAlign_Top; return PR_TRUE; } 
  virtual PRBool GetInitialAutoStretch(PRBool& aStretch)    { aStretch = PR_TRUE; return PR_TRUE; } 

  nsIBox* GetCaptionBox(nsPresContext* aPresContext, nsRect& aCaptionRect);
};

/*
class nsGroupBoxInnerFrame : public nsBoxFrame {
public:

    nsGroupBoxInnerFrame(nsIPresShell* aShell, nsStyleContext* aContext):
      nsBoxFrame(aShell, aContext) {}


#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("GroupBoxFrameInner", aResult);
  }
#endif
  
  // we are always flexible
  virtual PRBool GetDefaultFlex(PRInt32& aFlex) { aFlex = 1; return PR_TRUE; }

};
*/

nsIFrame*
NS_NewGroupBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsGroupBoxFrame(aPresShell, aContext);
}

class nsDisplayXULGroupBackground : public nsDisplayItem {
public:
  nsDisplayXULGroupBackground(nsGroupBoxFrame* aFrame) : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplayXULGroupBackground);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayXULGroupBackground() {
    MOZ_COUNT_DTOR(nsDisplayXULGroupBackground);
  }
#endif

  virtual nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt) { return mFrame; }
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("XULGroupBackground")
};

void
nsDisplayXULGroupBackground::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect)
{
  NS_STATIC_CAST(nsGroupBoxFrame*, mFrame)->
    PaintBorderBackground(*aCtx, aBuilder->ToReferenceFrame(mFrame), aDirtyRect);
}

NS_IMETHODIMP
nsGroupBoxFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsRect&           aDirtyRect,
                                  const nsDisplayListSet& aLists)
{
  // Paint our background and border
  if (IsVisibleForPainting(aBuilder)) {
    nsresult rv = aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
        nsDisplayXULGroupBackground(this));
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = DisplayOutline(aBuilder, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return BuildDisplayListForChildren(aBuilder, aDirtyRect, aLists);
  // REVIEW: Debug borders now painted by nsFrame::BuildDisplayListForChild
}

void
nsGroupBoxFrame::PaintBorderBackground(nsIRenderingContext& aRenderingContext,
    nsPoint aPt, const nsRect& aDirtyRect) {
  PRIntn skipSides = 0;
  const nsStyleBorder* borderStyleData = GetStyleBorder();
  const nsStylePadding* paddingStyleData = GetStylePadding();
  const nsMargin& border = borderStyleData->GetBorder();
  nscoord yoff = 0;
  nsPresContext* presContext = PresContext();

  nsRect groupRect;
  nsIBox* groupBox = GetCaptionBox(presContext, groupRect);

  if (groupBox) {        
    // if the border is smaller than the legend. Move the border down
    // to be centered on the legend. 
    nsMargin groupMargin;
    groupBox->GetStyleMargin()->GetMargin(groupMargin);
    groupRect.Inflate(groupMargin);
 
    if (border.top < groupRect.height)
        yoff = (groupRect.height - border.top)/2 + groupRect.y;
  }

  nsRect rect(aPt.x, aPt.y + yoff, mRect.width, mRect.height - yoff);

  groupRect += aPt;

  nsCSSRendering::PaintBackground(presContext, aRenderingContext, this,
                                  aDirtyRect, rect, *borderStyleData,
                                  *paddingStyleData, PR_FALSE);

  if (groupBox) {

    // we should probably use PaintBorderEdges to do this but for now just use clipping
    // to achieve the same effect.

    // draw left side
    nsRect clipRect(rect);
    clipRect.width = groupRect.x - rect.x;
    clipRect.height = border.top;

    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect);
    nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                aDirtyRect, rect, *borderStyleData, mStyleContext, skipSides);

    aRenderingContext.PopState();


    // draw right side
    clipRect = rect;
    clipRect.x = groupRect.XMost();
    clipRect.width = rect.XMost() - groupRect.XMost();
    clipRect.height = border.top;

    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect);
    nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                aDirtyRect, rect, *borderStyleData, mStyleContext, skipSides);

    aRenderingContext.PopState();

    
  
    // draw bottom

    clipRect = rect;
    clipRect.y += border.top;
    clipRect.height = mRect.height - (yoff + border.top);
  
    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect);
    nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                aDirtyRect, rect, *borderStyleData, mStyleContext, skipSides);

    aRenderingContext.PopState();
    
  } else {
    nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                aDirtyRect, nsRect(aPt, GetSize()),
                                *borderStyleData, mStyleContext, skipSides);
  }
}

nsIBox*
nsGroupBoxFrame::GetCaptionBox(nsPresContext* aPresContext, nsRect& aCaptionRect)
{
    // first child is our grouped area
    nsIBox* box = GetChildBox();

    // no area fail.
    if (!box)
      return nsnull;

    // get the first child in the grouped area, that is the caption
    box = box->GetChildBox();

    // nothing in the area? fail
    if (!box)
      return nsnull;

    // now get the caption itself. It is in the caption frame.
    nsIBox* child = box->GetChildBox();

    if (child) {
       // convert to our coordinates.
       nsRect parentRect(box->GetRect());
       aCaptionRect = child->GetRect();
       aCaptionRect.x += parentRect.x;
       aCaptionRect.y += parentRect.y;
    }

    return child;
}

NS_IMETHODIMP
nsGroupBoxFrame::GetBorderAndPadding(nsMargin& aBorderAndPadding)
{
  aBorderAndPadding.SizeTo(0,0,0,0);
  return NS_OK;
}

