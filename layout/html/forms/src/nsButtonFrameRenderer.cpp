#include "nsButtonFrameRenderer.h"
#include "nsIRenderingContext.h"
#include "nsCSSRendering.h"
#include "nsIPresContext.h"
#include "nsGenericHTMLElement.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsHTMLAtoms.h"

#define ACTIVE   "active"
#define HOVER    "hover"
#define FOCUS    "focus"

nsButtonFrameRenderer::nsButtonFrameRenderer()
{
	mNameSpace = kNameSpaceID_HTML;
}

nsButtonFrameRenderer::~nsButtonFrameRenderer()
{
}

void
nsButtonFrameRenderer::SetNameSpace(PRInt32 aNameSpace)
{
	mNameSpace = aNameSpace;
}

void
nsButtonFrameRenderer::SetFrame(nsFrame* aFrame, nsIPresContext& aPresContext)
{
	mFrame = aFrame;
	ReResolveStyles(aPresContext, 0, nsnull, nsnull);
}

void
nsButtonFrameRenderer::SetDisabled(PRBool aDisabled, PRBool notify)
{
   // get the content
  nsCOMPtr<nsIContent> content;
  mFrame->GetContent(getter_AddRefs(content));

  if (aDisabled)
     content->SetAttribute(mNameSpace, nsHTMLAtoms::disabled, "", notify);
  else
     content->UnsetAttribute(mNameSpace, nsHTMLAtoms::disabled, notify);

}

PRBool
nsButtonFrameRenderer::isDisabled() 
{
  // get the content
  nsCOMPtr<nsIContent> content;
  mFrame->GetContent(getter_AddRefs(content));
  nsString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(mNameSpace, nsHTMLAtoms::disabled, value))
    return PR_TRUE;

  return PR_FALSE;
}

void
nsButtonFrameRenderer::Redraw()
{
  nsRect rect;
  mFrame->GetRect(rect);
  rect.x = 0;
  rect.y = 0;
  mFrame->Invalidate(rect, PR_FALSE);
}

void 
nsButtonFrameRenderer::PaintButton     (nsIPresContext& aPresContext,
										nsIRenderingContext& aRenderingContext,
										const nsRect& aDirtyRect,
										nsFramePaintLayer aWhichLayer,
										const nsRect& aRect)
{
  //printf("painted width='%d' height='%d'\n",aRect.width, aRect.height);

	// draw the border and background inside the focus and outline borders
 	PaintBorderAndBackground(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, aRect);

	// draw the focus and outline borders
  PaintOutlineAndFocusBorders(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, aRect);
}

void
nsButtonFrameRenderer::PaintOutlineAndFocusBorders(nsIPresContext& aPresContext,
										nsIRenderingContext& aRenderingContext,
										const nsRect& aDirtyRect,
										nsFramePaintLayer aWhichLayer,
										const nsRect& aRect)
{
	// once we have all that let draw the focus if we have it. We will need to draw 2 focuses.
	// the inner and the outer. This is so we can do any kind of look and feel some buttons have
	// focus on the outside like mac and motif. While others like windows have it inside (dotted line).
	// Usually only one will be specifed. But I guess you could have both if you wanted to.

	nsRect rect;

  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) 
	{
		if (mOuterFocusStyle) {
			// ---------- paint the outer focus border -------------

 			GetButtonOuterFocusRect(aRect, rect);

			const nsStyleSpacing* spacing = (const nsStyleSpacing*)mOuterFocusStyle ->GetStyleData(eStyleStruct_Spacing);

			nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, mFrame,
									   aDirtyRect, rect, *spacing, mOuterFocusStyle, 0);
		}

		// ---------- paint the inner focus border -------------
		if (mInnerFocusStyle) { 

			GetButtonInnerFocusRect(aRect, rect);

			const nsStyleSpacing* spacing = (const nsStyleSpacing*)mInnerFocusStyle ->GetStyleData(eStyleStruct_Spacing);

			nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, mFrame,
									   aDirtyRect, rect, *spacing, mInnerFocusStyle, 0);
		}
	}

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) 
	{
    /*
		if (mOutlineStyle) {

			 GetButtonOutlineRect(aRect, rect);

       mOutlineRect = rect;

			 const nsStyleSpacing* spacing = (const nsStyleSpacing*)mOutlineStyle ->GetStyleData(eStyleStruct_Spacing);

			 // set the clipping area so we can draw outside our bounds.
 	 		 aRenderingContext.PushState();
			 PRBool clipEmpty;

		     aRenderingContext.SetClipRect(rect, nsClipCombine_kReplace, clipEmpty);
			 nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, mFrame,
							   aDirtyRect, rect, *spacing, mOutlineStyle, 0);

			 aRenderingContext.PopState(clipEmpty);
		}
    */
	}
}


void
nsButtonFrameRenderer::PaintBorderAndBackground(nsIPresContext& aPresContext,
										nsIRenderingContext& aRenderingContext,
										const nsRect& aDirtyRect,
										nsFramePaintLayer aWhichLayer,
										const nsRect& aRect)

{

    if (NS_FRAME_PAINT_LAYER_BACKGROUND != aWhichLayer) 
	   return;

	// get the button rect this is inside the focus and outline rects
	nsRect buttonRect;
	GetButtonRect(aRect, buttonRect);

	nsCOMPtr<nsIStyleContext> context;
    mFrame->GetStyleContext(getter_AddRefs(context));


	// get the styles
    const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)context->GetStyleData(eStyleStruct_Spacing);
    const nsStyleColor* color =
    (const nsStyleColor*)context->GetStyleData(eStyleStruct_Color);


	// paint the border and background

	nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, mFrame,
                                   aDirtyRect, buttonRect,  *color, *spacing, 0, 0);

    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, mFrame,
                               aDirtyRect, buttonRect, *spacing, context, 0);

}


void
nsButtonFrameRenderer::GetButtonOutlineRect(const nsRect& aRect, nsRect& outlineRect)
{
    outlineRect = aRect;
	outlineRect.Inflate(GetButtonOutlineBorderAndPadding());
}


void
nsButtonFrameRenderer::GetButtonOuterFocusRect(const nsRect& aRect, nsRect& focusRect)
{
    focusRect = aRect;
}

void
nsButtonFrameRenderer::GetButtonRect(const nsRect& aRect, nsRect& r)
{
    r = aRect;
	r.Deflate(GetButtonOuterFocusBorderAndPadding());
}


void
nsButtonFrameRenderer::GetButtonInnerFocusRect(const nsRect& aRect, nsRect& focusRect)
{
    GetButtonRect(aRect, focusRect);
	focusRect.Deflate(GetButtonBorderAndPadding());
    focusRect.Deflate(GetButtonInnerFocusMargin());
}

void
nsButtonFrameRenderer::GetButtonContentRect(const nsRect& aRect, nsRect& r)
{
	GetButtonInnerFocusRect(aRect, r);
	r.Deflate(GetButtonInnerFocusBorderAndPadding());
}


nsMargin
nsButtonFrameRenderer::GetButtonOuterFocusBorderAndPadding()
{
	nsMargin focusBorderAndPadding(0,0,0,0);

	if (mOuterFocusStyle) {
		// get the outer focus border and padding
		const nsStyleSpacing* spacing = (const nsStyleSpacing*)mOuterFocusStyle ->GetStyleData(eStyleStruct_Spacing);
		if (!spacing->GetBorderPadding(focusBorderAndPadding)) {
      NS_NOTYETIMPLEMENTED("percentage border");
    }
	}

	return focusBorderAndPadding;
}

nsMargin
nsButtonFrameRenderer::GetButtonBorderAndPadding()
{
	nsCOMPtr<nsIStyleContext> context;
	mFrame->GetStyleContext(getter_AddRefs(context));

    nsMargin borderAndPadding(0,0,0,0);
 	const nsStyleSpacing* spacing = (const nsStyleSpacing*)context ->GetStyleData(eStyleStruct_Spacing);
    if (!spacing->GetBorderPadding(borderAndPadding)) {
      NS_NOTYETIMPLEMENTED("percentage border");
    }
	return borderAndPadding;
}

/**
 * Gets the size of the buttons border this is the union of the normal and disabled borders.
 */
nsMargin
nsButtonFrameRenderer::GetButtonInnerFocusMargin()
{
	nsMargin innerFocusMargin(0,0,0,0);

	if (mInnerFocusStyle) {
		// get the outer focus border and padding
		const nsStyleSpacing* spacing = (const nsStyleSpacing*)mInnerFocusStyle ->GetStyleData(eStyleStruct_Spacing);
		spacing->GetMargin(innerFocusMargin);
	}

	return innerFocusMargin;
}

nsMargin
nsButtonFrameRenderer::GetButtonInnerFocusBorderAndPadding()
{
	nsMargin innerFocusBorderAndPadding(0,0,0,0);

	if (mInnerFocusStyle) {
		// get the outer focus border and padding
		const nsStyleSpacing* spacing = (const nsStyleSpacing*)mInnerFocusStyle ->GetStyleData(eStyleStruct_Spacing);
		if (!spacing->GetBorderPadding(innerFocusBorderAndPadding)) {
      NS_NOTYETIMPLEMENTED("percentage border");
    }
	}

	return innerFocusBorderAndPadding;
}

nsMargin
nsButtonFrameRenderer::GetButtonOutlineBorderAndPadding()
{
	nsMargin borderAndPadding(0,0,0,0);

	if (mOutlineStyle) {
		// get the outline border and padding
		const nsStyleSpacing* spacing = (const nsStyleSpacing*)mOutlineStyle ->GetStyleData(eStyleStruct_Spacing);
		if (!spacing->GetBorderPadding(borderAndPadding)) {
      NS_NOTYETIMPLEMENTED("percentage border");
    }
	}

	return borderAndPadding;
}

// gets the full size of our border with all the focus borders
nsMargin
nsButtonFrameRenderer::GetFullButtonBorderAndPadding()
{
  return GetAddedButtonBorderAndPadding() + GetButtonBorderAndPadding();
}

// gets all the focus borders and padding that will be added to the regular border
nsMargin
nsButtonFrameRenderer::GetAddedButtonBorderAndPadding()
{
  return GetButtonOuterFocusBorderAndPadding() + GetButtonInnerFocusMargin() + GetButtonInnerFocusBorderAndPadding();
}

/**
 * Call this when styles change
 */
void 
nsButtonFrameRenderer::ReResolveStyles(nsIPresContext& aPresContext,
                                       PRInt32 aParentChange,
                                       nsStyleChangeList* aChangeList,
                                       PRInt32* aLocalChange)
{

	// get all the styles
	nsCOMPtr<nsIContent> content;
	mFrame->GetContent(getter_AddRefs(content));
	nsCOMPtr<nsIStyleContext> context;
	mFrame->GetStyleContext(getter_AddRefs(context));

  // style that draw an outline around the button

  // see if the outline has changed.
  nsCOMPtr<nsIStyleContext> oldOutline = mOutlineStyle;

	nsCOMPtr<nsIAtom> atom ( getter_AddRefs(NS_NewAtom(":-moz-outline")) );
	aPresContext.ProbePseudoStyleContextFor(content, atom, context,
										  PR_FALSE,
										  getter_AddRefs(mOutlineStyle));

  if ((mOutlineStyle && oldOutline) && (mOutlineStyle != oldOutline)) {
    nsFrame::CaptureStyleChangeFor(mFrame, oldOutline, mOutlineStyle, 
                              aParentChange, aChangeList, aLocalChange);
  }

    // style for the inner such as a dotted line (Windows)
	atom = getter_AddRefs(NS_NewAtom(":-moz-focus-inner"));
  nsCOMPtr<nsIStyleContext> oldInnerFocus = mInnerFocusStyle;
	aPresContext.ProbePseudoStyleContextFor(content, atom, context,
										  PR_FALSE,
										  getter_AddRefs(mInnerFocusStyle));

  if ((mInnerFocusStyle && oldInnerFocus) && (mInnerFocusStyle != oldInnerFocus)) {
    nsFrame::CaptureStyleChangeFor(mFrame, oldInnerFocus, mInnerFocusStyle, 
                                   aParentChange, aChangeList, aLocalChange);
  }


	// style for outer focus like a ridged border (MAC).
	atom = getter_AddRefs(NS_NewAtom(":-moz-focus-outer"));
  nsCOMPtr<nsIStyleContext> oldOuterFocus = mOuterFocusStyle;
	aPresContext.ProbePseudoStyleContextFor(content, atom, context,
										  PR_FALSE,
										  getter_AddRefs(mOuterFocusStyle));

  if ((mOuterFocusStyle && oldOuterFocus) && (mOuterFocusStyle != oldOuterFocus)) {
    nsFrame::CaptureStyleChangeFor(mFrame, oldOuterFocus, mOuterFocusStyle, 
                                   aParentChange, aChangeList, aLocalChange);
  }
}
