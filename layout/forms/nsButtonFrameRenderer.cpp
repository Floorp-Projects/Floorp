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
nsButtonFrameRenderer::SetFrame(nsIFrame* aFrame, nsIPresContext& aPresContext)
{
	mFrame = aFrame;
	ReResolveStyles(aPresContext);
}


nsString
nsButtonFrameRenderer::GetPseudoClassAttribute()
{
  // get the content
  nsCOMPtr<nsIContent> content;
  mFrame->GetContent(getter_AddRefs(content));
	
  // create and atom for the pseudo class
  nsCOMPtr<nsIAtom> atom = do_QueryInterface(NS_NewAtom("pseudoclass"));

  // get the attribute
  nsAutoString value;
  content->GetAttribute(mNameSpace, atom, value);

  /*
  char ch[256];
  value.ToCString(ch,256);
  printf("getting pseudo='%s'\n",ch);
  */

  return value;
}


void
nsButtonFrameRenderer::SetPseudoClassAttribute(const nsString& value, PRBool notify)
{
  // get the content
  nsCOMPtr<nsIContent> content;
  mFrame->GetContent(getter_AddRefs(content));
	
  // create and atom for the pseudo class
  nsCOMPtr<nsIAtom> atom = do_QueryInterface(NS_NewAtom("pseudoclass"));

  nsString pseudo = value;
  // remove whitespace
  pseudo.Trim(" ");
  pseudo.CompressWhitespace();

  // set it
  content->SetAttribute(mNameSpace, atom, pseudo, notify);
  
  /*
  char ch[256];
  pseudo.ToCString(ch,256);
  printf("setting pseudo='%s'\n",ch);
  */
}

void
nsButtonFrameRenderer::SetHover(PRBool aHover, PRBool notify)
{
   ToggleClass(aHover, HOVER, notify);
}

void
nsButtonFrameRenderer::SetActive(PRBool aActive, PRBool notify)
{
   ToggleClass(aActive, ACTIVE, notify);
}

void
nsButtonFrameRenderer::SetFocus(PRBool aFocus, PRBool notify)
{
   ToggleClass(aFocus, FOCUS, notify);
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
nsButtonFrameRenderer::isHover() 
{
	nsString pseudo = GetPseudoClassAttribute();
	PRInt32 index = IndexOfClass(pseudo, HOVER);
    if (index != -1)
		return PR_TRUE;
	else
		return PR_FALSE;
}

PRBool
nsButtonFrameRenderer::isActive() 
{
	nsString pseudo = GetPseudoClassAttribute();
	PRInt32 index = IndexOfClass(pseudo, ACTIVE);
    if (index != -1)
		return PR_TRUE;
	else
		return PR_FALSE;
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

PRBool
nsButtonFrameRenderer::isFocus() 
{
	nsString pseudo = GetPseudoClassAttribute();
	PRInt32 index = IndexOfClass(pseudo, FOCUS);
    if (index != -1)
		return PR_TRUE;
	else
		return PR_FALSE;
}

void
nsButtonFrameRenderer::ToggleClass(PRBool aValue, const nsString& c, PRBool notify)
{
   // get the pseudo class
   nsString pseudo = GetPseudoClassAttribute();

   // if focus add it 
   if (aValue) 
	   AddClass(pseudo, c);
   else
	   RemoveClass(pseudo, c);

   // set pseudo class
   SetPseudoClassAttribute(pseudo, notify);
}


void
nsButtonFrameRenderer::AddClass(nsString& pseudoclass, const nsString newClass)
{
	// see if the class is already there
	// if not add it

	PRInt32 index = IndexOfClass(pseudoclass, newClass);
    if (index == -1) {
       pseudoclass += " ";
	   pseudoclass += newClass;
	}
}

void
nsButtonFrameRenderer::RemoveClass(nsString& pseudoclass, const nsString newClass)
{
	// see if the class is there
	// if so remove it
	PRInt32 index = IndexOfClass(pseudoclass, newClass);
    if (index == -1)
		return;

    // remove it
	pseudoclass.Cut(index, newClass.Length());
}

PRInt32
nsButtonFrameRenderer::IndexOfClass(nsString& pseudoclass, const nsString& c)
{
	// easy first case
	if (pseudoclass.Equals(c))
       return 0;
 
	// look on left
	PRInt32 index = pseudoclass.Find(nsString(c) + " ");
	if (index == -1 || index > 0) {
		// look on right
        index = pseudoclass.Find(nsString(" ") + c);
	    if (index == -1 || index != pseudoclass.Length() - (c.Length()+1))
		{
			// look in center
			index = pseudoclass.Find(nsString(" ") + c + " ");
			if (index == -1)
				return -1;
			else
				index++;
		} else 
			index++;
	}

	

	return index;
}



NS_IMETHODIMP
nsButtonFrameRenderer::HandleEvent(nsIPresContext& aPresContext, 
                                  nsGUIEvent* aEvent,
                                  nsEventStatus& aEventStatus)
{
  // if disabled do nothing
  if (PR_TRUE == isDisabled()) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> content;
  mFrame->GetContent(getter_AddRefs(content));

  // get its view
  nsIView* view = nsnull;
  mFrame->GetView(&view);
  nsCOMPtr<nsIViewManager> viewMan;

  if (view)
    view->GetViewManager(*getter_AddRefs(viewMan));

  aEventStatus = nsEventStatus_eIgnore;
 
  switch (aEvent->message) {

    case NS_MOUSE_ENTER:
     SetHover(PR_TRUE, PR_TRUE);
    break;

    case NS_MOUSE_LEFT_BUTTON_DOWN: 
      SetActive(PR_TRUE, PR_TRUE);
      // grab all mouse events

      PRBool result;
      if (viewMan)
      viewMan->GrabMouseEvents(view,result);
    break;

    case NS_MOUSE_LEFT_BUTTON_UP:
      SetActive(PR_FALSE, PR_TRUE);
      // stop grabbing mouse events
      if (viewMan)
      viewMan->GrabMouseEvents(nsnull,result);
    break;
      case NS_MOUSE_EXIT:
      // if we don't have a view then we might not know when they release
      // the button. So on exit go back to the normal state.
      if (!viewMan)
        SetActive(PR_FALSE, PR_TRUE);

      SetHover(PR_FALSE, PR_TRUE);
      
      break;
    }

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
		if /*(mOutlineStyle) */ (PR_FALSE) {

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

nsMargin
nsButtonFrameRenderer::GetFullButtonBorderAndPadding()
{
  return GetButtonOuterFocusBorderAndPadding() + GetButtonBorderAndPadding() + GetButtonInnerFocusMargin() + GetButtonInnerFocusBorderAndPadding();
}

void 
nsButtonFrameRenderer::ReResolveStyles(nsIPresContext& aPresContext)
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
nsButtonFrameRenderer::AddFocusBordersAndPadding(nsIPresContext& aPresContext,
                                  const nsHTMLReflowState& aReflowState,
                                  nsHTMLReflowMetrics& aMetrics,
                                  nsMargin& aBorderPadding)
{
  aBorderPadding = aReflowState.mComputedBorderPadding;
  
  nsMargin m = GetButtonOuterFocusBorderAndPadding();
  m += GetButtonInnerFocusMargin();
  m += GetButtonInnerFocusBorderAndPadding();

  aBorderPadding += m;
 
  aMetrics.width += m.left + m.right;
  aMetrics.height += m.top + m.bottom;
  
  aMetrics.ascent = aMetrics.height;
  aMetrics.descent = 0;

 // printf("requested width='%d' height='%d'\n",aMetrics.width, aMetrics.height);
}


