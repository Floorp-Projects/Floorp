#include "nsButtonFrameRenderer.h"
#include "nsIRenderingContext.h"
#include "nsCSSRendering.h"
#include "nsIPresContext.h"
#include "nsGenericHTMLElement.h"

nsButtonFrameRenderer::nsButtonFrameRenderer()
{
	mState = normal;
        mFocus = PR_FALSE;
	mEnabled = PR_TRUE;
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
nsButtonFrameRenderer::SetFrame(nsIFrame* aFrame, nsIPresContext& aPresContext)
{
	mFrame = aFrame;
	UpdateStyles(aPresContext);
}

void
nsButtonFrameRenderer::Update(PRBool notify)
{
  nsCOMPtr<nsIContent> content;
  mFrame->GetContent(getter_AddRefs(content));

  nsString state ="";

  switch (mState)
  {
      case hover:
         state = "hover";
		 break;
	  case active:
		 state = "active";
		 break;
	  case normal:
		 state = "";
		 break;
  }

  nsString enabled;
  
  if (mEnabled)
	  enabled = "enabled";
  else
	  enabled = "disabled";
 
  nsString focus;
  if (mFocus)
	  focus = "focus";
  else
	  focus = "";

  nsString value = state + " " + focus + " " + enabled;
  value.Trim(" ");
  value.CompressWhitespace();

  /*
  char ch[256];
  value.ToCString(ch,256);
  printf("selector='%s'\n",ch);
  */

  nsCOMPtr<nsIAtom> atom = do_QueryInterface(NS_NewAtom("pseudoclass"));
  content->SetAttribute(mNameSpace, atom, value, notify);
}

void
nsButtonFrameRenderer::SetState(ButtonState state)
{
   mState = state;
}

void
nsButtonFrameRenderer::SetFocus(PRBool aFocus)
{
   mFocus = aFocus;
}

void
nsButtonFrameRenderer::SetEnabled(PRBool aEnabled)
{
   mEnabled = aEnabled;
}


NS_IMETHODIMP
nsButtonFrameRenderer::HandleEvent(nsIPresContext& aPresContext, 
                                  nsGUIEvent* aEvent,
                                  nsEventStatus& aEventStatus)
{
  // if disabled do nothing
  if (nsnull == mEnabled) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> content;
  mFrame->GetContent(getter_AddRefs(content));

/* View support has removed because views don't seem to be supporting
   Transpancy and the manager isn't supporting event grabbing either.
  // get its view
  nsIView* view = nsnull;
  GetView(&view);
  nsCOMPtr<nsIViewManager> viewMan;
  view->GetViewManager(*getter_AddRefs(viewMan));
*/

  aEventStatus = nsEventStatus_eIgnore;
 
  switch (aEvent->message) {

        case NS_MOUSE_ENTER:
            SetState(hover);
			Update(PR_TRUE);
	      break;
        case NS_MOUSE_LEFT_BUTTON_DOWN: 
 			SetState(active);
 			Update(PR_TRUE);
		  // grab all mouse events
		  
		 // PRBool result;
		  //viewMan->GrabMouseEvents(view,result);
		  break;

        case NS_MOUSE_LEFT_BUTTON_UP:
			SetState(hover);
 			Update(PR_TRUE);
			// stop grabbing mouse events
            //viewMan->GrabMouseEvents(nsnull,result);
	        break;
        case NS_MOUSE_EXIT:
			SetState(normal);
			Update(PR_TRUE);
	        break;
  }

  aEventStatus = nsEventStatus_eConsumeNoDefault;

  return NS_OK;
}

/*
void
nsButtonFrameRenderer::Redraw()
{	
	nsRect frameRect;
	mFrame->GetRect(frameRect);
	nsRect rect(0, 0, frameRect.width, frameRect.height);
    mFrame->Invalidate(rect, PR_TRUE);
}
*/

void 
nsButtonFrameRenderer::PaintButton     (nsIPresContext& aPresContext,
										nsIRenderingContext& aRenderingContext,
										const nsRect& aDirtyRect,
										nsFramePaintLayer aWhichLayer,
										const nsRect& aRect)
{
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

    if (eFramePaintLayer_Underlay == aWhichLayer) 
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

    if (eFramePaintLayer_Overlay == aWhichLayer) 
	{
		if (mOutlineStyle) {

			 GetButtonOutlineRect(aRect, rect);
 
			 const nsStyleSpacing* spacing = (const nsStyleSpacing*)mOutlineStyle ->GetStyleData(eStyleStruct_Spacing);

			 // set the clipping area so we can draw outside our bounds.
 	 		 aRenderingContext.PushState();
			 PRBool clipEmpty;

		     aRenderingContext.SetClipRect(rect, nsClipCombine_kReplace, clipEmpty);
			 nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, mFrame,
							   aDirtyRect, rect, *spacing, mOutlineStyle, 0);

			 aRenderingContext.PopState(clipEmpty);
		}
	}
}


void
nsButtonFrameRenderer::PaintBorderAndBackground(nsIPresContext& aPresContext,
										nsIRenderingContext& aRenderingContext,
										const nsRect& aDirtyRect,
										nsFramePaintLayer aWhichLayer,
										const nsRect& aRect)

{

    if (eFramePaintLayer_Underlay != aWhichLayer) 
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
		spacing->GetBorderPadding(focusBorderAndPadding);
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
    spacing->GetBorderPadding(borderAndPadding);
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
		spacing->GetBorderPadding(innerFocusBorderAndPadding);
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
		spacing->GetBorderPadding(borderAndPadding);
	}

	return borderAndPadding;
}

void 
nsButtonFrameRenderer::UpdateStyles(nsIPresContext& aPresContext)
{
	// get all the styles
	nsCOMPtr<nsIContent> content;
	mFrame->GetContent(getter_AddRefs(content));
	nsCOMPtr<nsIStyleContext> context;
	mFrame->GetStyleContext(getter_AddRefs(context));

    // style that draw an outline around the button
	nsCOMPtr<nsIAtom> atom (do_QueryInterface(NS_NewAtom(":-moz-outline")));
	aPresContext.ProbePseudoStyleContextFor(content, atom, context,
										  PR_FALSE,
										  getter_AddRefs(mOutlineStyle));


    // style for the inner such as a dotted line (Windows)
	atom = do_QueryInterface(NS_NewAtom(":-moz-focus-inner"));
	aPresContext.ProbePseudoStyleContextFor(content, atom, context,
										  PR_FALSE,
										  getter_AddRefs(mInnerFocusStyle));



	// style for outer focus like a ridged border (MAC).
	atom = do_QueryInterface(NS_NewAtom(":-moz-focus-outer"));
	aPresContext.ProbePseudoStyleContextFor(content, atom, context,
										  PR_FALSE,
										  getter_AddRefs(mOuterFocusStyle));
}


void
nsButtonFrameRenderer::AddBordersAndPadding(nsIPresContext* aPresContext,
                                  const nsHTMLReflowState& aReflowState,
                                  nsHTMLReflowMetrics& aMetrics,
                                  nsMargin& aBorderPadding)
{
	/*
  nsHTMLReflowState::ComputeBorderPaddingFor(aFrame,
                                             aReflowState.parentReflowState,
                                             aBorderPadding);
  */

  aBorderPadding.left = aBorderPadding.right = aBorderPadding.top = aBorderPadding.bottom = 0;
 
  aBorderPadding += GetButtonOuterFocusBorderAndPadding();
  aBorderPadding += GetButtonBorderAndPadding();
  aBorderPadding += GetButtonInnerFocusMargin();
  aBorderPadding += GetButtonInnerFocusBorderAndPadding();
 
  aMetrics.width += aBorderPadding.left + aBorderPadding.right;
  aMetrics.height += aBorderPadding.top + aBorderPadding.bottom;
}


