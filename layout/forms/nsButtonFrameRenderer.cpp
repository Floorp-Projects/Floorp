/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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


#include "nsButtonFrameRenderer.h"

#include "nsFileControlFrame.h"
#include "nsFormControlHelper.h"
#include "nsHTMLParts.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsHTMLIIDs.h"
#include "nsIButton.h"
#include "nsIViewManager.h"
#include "nsISupports.h"
#include "nsHTMLAtoms.h"
#include "nsIButton.h"
#include "nsIView.h"
#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsIFormControl.h"
#include "nsStyleUtil.h"
#include "nsDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsFormFrame.h"
#include "nsILookAndFeel.h"
#include "nsCOMPtr.h"
#include "nsDOMEvent.h"
#include "nsIViewManager.h"


nsButtonFrameRenderer::nsButtonFrameRenderer()
{
	mDisabledChanged = PR_FALSE;
	mPressed = PR_FALSE;
    mGotFocus = PR_FALSE;
    mDisabled = PR_FALSE;

	// default namespace is html
	mNameSpace = kNameSpaceID_HTML;
}

void
nsButtonFrameRenderer::SetNameSpace(PRInt32 aNameSpaceID)
{
	mNameSpace = aNameSpaceID;
}

nscoord 
nsButtonFrameRenderer::GetHorizontalInsidePadding(const nsFrame* aFrame, 
												 nsIPresContext& aPresContext,
                                                 float aPixToTwip, 
                                                 nscoord aInnerWidth,
                                                 nscoord aCharWidth) const
{
    return NSIntPixelsToTwips(3, aPixToTwip);
}

nscoord 
nsButtonFrameRenderer::GetVerticalInsidePadding(const nsFrame* aFrame,
												float aPixToTwip, 
                                                nscoord aInnerHeight) const
{
	return NSIntPixelsToTwips(3, aPixToTwip);
}

NS_IMETHODIMP
nsButtonFrameRenderer::HandleEvent(nsFrame* aFrame,
								  nsIPresContext& aPresContext, 
                                  nsGUIEvent* aEvent,
                                  nsEventStatus& aEventStatus)
{
  // if disabled do nothing
  if (mDisabled) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));

/* View support has removed because views don't seem to be supporting
   Transpancy and the manager isn't supporting event grabbing either.
  // get its view
  nsIView* view = nsnull;
  GetView(&view);
  nsCOMPtr<nsIViewManager> viewMan;
  view->GetViewManager(*getter_AddRefs(viewMan));
*/

  aEventStatus = nsEventStatus_eIgnore;
  nsresult result = NS_OK;
 
  switch (aEvent->message) {

        case NS_MOUSE_ENTER:
		  mMouseInside = PR_TRUE;
		  if (mPressed)
			   content->SetAttribute(mNameSpace, nsHTMLAtoms::kClass, "pressed", PR_TRUE);
          else
               content->SetAttribute(mNameSpace, nsHTMLAtoms::kClass, "rollover", PR_TRUE);

	      break;
        case NS_MOUSE_LEFT_BUTTON_DOWN: 
          mGotFocus = PR_TRUE;
		  mPressed = PR_TRUE;
		  content->SetAttribute(mNameSpace, nsHTMLAtoms::kClass, "pressed", PR_TRUE);

		  // grab all mouse events
		  
		 // PRBool result;
		  //viewMan->GrabMouseEvents(view,result);
		  break;

        case NS_MOUSE_LEFT_BUTTON_UP:
		    mPressed = PR_FALSE;
			content->SetAttribute(mNameSpace, nsHTMLAtoms::kClass, "", PR_TRUE);

			// stop grabbing mouse events
            //viewMan->GrabMouseEvents(nsnull,result);
	        break;
        case NS_MOUSE_EXIT:
			content->SetAttribute(mNameSpace, nsHTMLAtoms::kClass, "", PR_TRUE);
		    mMouseInside = PR_FALSE;

		  // KLUDGE make is false when you exit because grabbing mouse events doesn't
		  // seem to work. If it did we would know when the mouse was released outside of
		  // us. And we could set this to false.
		  mPressed = PR_FALSE;
	      break;
  }

  aEventStatus = nsEventStatus_eConsumeNoDefault;

  return NS_OK;
}

void 
nsButtonFrameRenderer::PaintButton(nsFrame* aFrame,
										nsIPresContext& aPresContext,
										nsIRenderingContext& aRenderingContext,
										const nsRect& aDirtyRect,
										nsFramePaintLayer aWhichLayer,
										const nsString& aLabel,
										const nsRect& aRect)
{
   PaintButtonBorder(aFrame,
	                 aPresContext,
					 aRenderingContext,
					 aDirtyRect,
					 aWhichLayer,
					 aRect);

    PaintButtonContents(aFrame,
	                 aPresContext,
					 aRenderingContext,
					 aDirtyRect,
					 aWhichLayer,
					 aLabel,
					 aRect);
}

void 
nsButtonFrameRenderer::PaintButtonBorder(nsFrame* aFrame,
										nsIPresContext& aPresContext,
										nsIRenderingContext& aRenderingContext,
										const nsRect& aDirtyRect,
										nsFramePaintLayer aWhichLayer,
										const nsRect& aRect)
{
    nsCOMPtr<nsIStyleContext> context;
    aFrame->GetStyleContext(getter_AddRefs(context));

	nsCOMPtr<nsIContent> content;
    aFrame->GetContent(getter_AddRefs(content));

    CheckDisabled(content);

   // Draw border using CSS
     // Get the Scrollbar's Arrow's Style structs
    const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)context->GetStyleData(eStyleStruct_Spacing);
    const nsStyleColor* color =
    (const nsStyleColor*)context->GetStyleData(eStyleStruct_Color);

	
    nsRect rect(aRect);


	const nsStyleSpacing* outline = (const nsStyleSpacing*)mOutlineStyle->GetStyleData(eStyleStruct_Spacing);
  	const nsStyleSpacing* focus = (const nsStyleSpacing*)mFocusStyle->GetStyleData(eStyleStruct_Spacing);
 
	nsMargin outlineBorder;
    outline->CalcBorderFor(aFrame, outlineBorder);

 	nsMargin focusBorder;
    focus->CalcBorderFor(aFrame, focusBorder);

	int left = (focusBorder.left > outlineBorder.left) ? focusBorder.left : outlineBorder.left;
	int right = (focusBorder.right > outlineBorder.right) ? focusBorder.right : outlineBorder.right;
	int top = (focusBorder.top > outlineBorder.top) ? focusBorder.top : outlineBorder.top;
	int bottom = (focusBorder.bottom > outlineBorder.bottom) ? focusBorder.bottom : outlineBorder.bottom;

	if (mGotFocus) {
		int l = left - focusBorder.left;
		int t = top - focusBorder.top;
		int b = bottom - focusBorder.bottom;
		int r = right - focusBorder.right;

		int w = aRect.width - (l + r);
		int h = aRect.height - (t + b);

		nsRect focusRect(l, t, w, h);

		nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aFrame,
								   aDirtyRect, focusRect, *focus, context, 0);
	} 
	
	if (mMouseInside) {
		int l = left - outlineBorder.left;
		int t = top - outlineBorder.top;
		int b = bottom - outlineBorder.bottom;
		int r = right - outlineBorder.right;

		int w = aRect.width - (l + r);
		int h = aRect.height - (t + b);

		nsRect outlineRect(l, t, w, h);

		nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aFrame,
								   aDirtyRect, outlineRect, *outline, context, 0);
	}

	rect.x += left;
	rect.y += right;
	rect.width -= (left + right);
	rect.height -= (top + bottom);

	nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, aFrame,
                                   aDirtyRect, rect,  *color, *spacing, 0, 0);

    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aFrame,
                               aDirtyRect, rect, *spacing, context, 0);
}


void 
nsButtonFrameRenderer::PaintButtonContents(nsFrame* aFrame,nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nsFramePaintLayer aWhichLayer,
							const nsString& aLabel,
							const nsRect& aRect)
{
    nsCOMPtr<nsIStyleContext> context;
    aFrame->GetStyleContext(getter_AddRefs(context));

    nsCOMPtr<nsIContent> content;
    aFrame->GetContent(getter_AddRefs(content));

    CheckDisabled(content);

    aRenderingContext.PushState();

	// get the border and the outer border (union of focus border, and outline border)
    const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)context->GetStyleData(eStyleStruct_Spacing);
    const nsStyleColor* color =
    (const nsStyleColor*)context->GetStyleData(eStyleStruct_Color);

	const nsStyleSpacing* outline = (const nsStyleSpacing*)mOutlineStyle->GetStyleData(eStyleStruct_Spacing);
  	const nsStyleSpacing* focus = (const nsStyleSpacing*)mFocusStyle->GetStyleData(eStyleStruct_Spacing);
 
	nsMargin outlineBorder;
	nsMargin focusBorder;
	nsMargin outerBorder;

	nsMargin border;

    outline->CalcBorderFor(aFrame, outlineBorder);
    focus->CalcBorderFor(aFrame, focusBorder);

	MarginUnion(outlineBorder, focusBorder, outerBorder);

    spacing->CalcBorderFor(aFrame, border);

    float p2t;
    aPresContext.GetScaledPixelsToTwips(&p2t);
    nscoord onePixel = NSIntPixelsToTwips(1, p2t);

    nsRect outside(aRect);
    outside.Deflate(border);
	outside.Deflate(outerBorder);
    outside.Deflate(onePixel, onePixel);

    nsRect inside(outside);
    inside.Deflate(onePixel, onePixel);

	aRenderingContext.SetColor(NS_RGB(255,255,255));

    float appUnits;
    float devUnits;
    float scale;

	nsCOMPtr<nsIDeviceContext> dcontext;
    aRenderingContext.GetDeviceContext(*getter_AddRefs(dcontext));

    dcontext->GetCanonicalPixelScale(scale);
    dcontext->GetAppUnitsToDevUnits(devUnits);
    dcontext->GetDevUnitsToAppUnits(appUnits);

    nscoord ascent;
    nscoord descent;
  
    nscoord textWidth;
    nscoord textHeight;
    aRenderingContext.GetWidth(aLabel, textWidth);

    nsIFontMetrics* metrics;
    aRenderingContext.GetFontMetrics(metrics);

    metrics->GetMaxAscent(ascent);
    metrics->GetMaxDescent(descent);
    textHeight = ascent + descent;

    nscoord x = ((inside.width  - textWidth) / 2)  + inside.x;
    nscoord y = ((inside.height - textHeight) / 2) + inside.y;
    if (PR_TRUE == mPressed) {
      x += onePixel;
      y += onePixel;
    }

	if (mDisabled) {
	   aRenderingContext.SetColor(NS_RGB(255,255,255));
       aRenderingContext.DrawString(aLabel, x+onePixel, y+onePixel);
	}

	aRenderingContext.SetColor(color->mColor);
    aRenderingContext.DrawString(aLabel, x, y);

    PRBool clipEmpty;
    aRenderingContext.PopState(clipEmpty);
}

void 
nsButtonFrameRenderer::CheckDisabled(nsIContent* content)
{
	
  if ( mDisabledChanged )
  {
    if (mDisabled)
      content->SetAttribute(mNameSpace, nsHTMLAtoms::kClass, "DISABLED", PR_TRUE);
    else 
      content->SetAttribute(mNameSpace, nsHTMLAtoms::kClass, "", PR_TRUE);

	mDisabledChanged = PR_FALSE;
  }
}

void
nsButtonFrameRenderer::MarginUnion(const nsMargin& a, const nsMargin& b, nsMargin& ab)
{
	
	ab.left = (a.left > b.left) ? a.left : b.left;
	ab.right = (a.right > b.right) ? a.right : b.right;
	ab.top = (a.top > b.top) ? a.top : b.top;
	ab.bottom = (a.bottom > b.bottom) ? a.bottom : b.bottom;
}

void 
nsButtonFrameRenderer::ResetButtonStyles()
{
	mOutlineStyle = do_QueryInterface(nsnull);
	mFocusStyle = do_QueryInterface(nsnull);
}

void 
nsButtonFrameRenderer::UpdateButtonStyles(nsFrame* aFrame, nsIPresContext& aPresContext)
{
	nsCOMPtr<nsIContent> content;
    aFrame->GetContent(getter_AddRefs(content));
    nsCOMPtr<nsIStyleContext> context;
    aFrame->GetStyleContext(getter_AddRefs(context));

	if (!mOutlineStyle) {
		  nsCOMPtr<nsIAtom> atom (do_QueryInterface(NS_NewAtom(":button-outline")));
		  aPresContext.ProbePseudoStyleContextFor(content, atom, context,
												  PR_FALSE,
												  getter_AddRefs(mOutlineStyle));
	}

	if (!mFocusStyle) {
		  nsCOMPtr<nsIAtom> atom (do_QueryInterface(NS_NewAtom(":button-focus")));
		  aPresContext.ProbePseudoStyleContextFor(content, atom, context,
												  PR_FALSE,
												  getter_AddRefs(mFocusStyle));
	}
}

void
nsButtonFrameRenderer::SetDisabled(PRBool aDisabled)
{
	mDisabledChanged = PR_TRUE;
	mDisabled = aDisabled;
}

void
nsButtonFrameRenderer::SetFocus(PRBool aGotFocus)
{
	mGotFocus = aGotFocus;
}

void
nsButtonFrameRenderer::GetButtonMargin(nsFrame* aFrame, nsMargin& aMargin)
{
	nsMargin border;
	const nsStyleSpacing* outline = (const nsStyleSpacing*)mOutlineStyle->GetStyleData(eStyleStruct_Spacing);
	const nsStyleSpacing* focus = (const nsStyleSpacing*)mFocusStyle->GetStyleData(eStyleStruct_Spacing);

	nsMargin outlineBorder;
	nsMargin focusBorder;

	outline->CalcBorderFor(aFrame, outlineBorder);
	focus->CalcBorderFor(aFrame, focusBorder);

	MarginUnion(outlineBorder, focusBorder, aMargin);
}
