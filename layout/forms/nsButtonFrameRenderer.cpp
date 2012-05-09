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
 * The Original Code is Mozilla Communicator client code.
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
#include "nsButtonFrameRenderer.h"
#include "nsCSSRendering.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsCSSPseudoElements.h"
#include "nsINameSpaceManager.h"
#include "nsStyleSet.h"
#include "nsDisplayList.h"
#include "nsITheme.h"
#include "nsThemeConstants.h"
#include "nsEventStates.h"
#include "mozilla/dom/Element.h"

#define ACTIVE   "active"
#define HOVER    "hover"
#define FOCUS    "focus"

nsButtonFrameRenderer::nsButtonFrameRenderer()
{
  MOZ_COUNT_CTOR(nsButtonFrameRenderer);
}

nsButtonFrameRenderer::~nsButtonFrameRenderer()
{
  MOZ_COUNT_DTOR(nsButtonFrameRenderer);
}

void
nsButtonFrameRenderer::SetFrame(nsFrame* aFrame, nsPresContext* aPresContext)
{
  mFrame = aFrame;
  ReResolveStyles(aPresContext);
}

nsIFrame*
nsButtonFrameRenderer::GetFrame()
{
  return mFrame;
}

void
nsButtonFrameRenderer::SetDisabled(bool aDisabled, bool notify)
{
  if (aDisabled)
    mFrame->GetContent()->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled, EmptyString(),
                                  notify);
  else
    mFrame->GetContent()->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, notify);
}

bool
nsButtonFrameRenderer::isDisabled() 
{
  return mFrame->GetContent()->AsElement()->
    State().HasState(NS_EVENT_STATE_DISABLED);
}

class nsDisplayButtonBoxShadowOuter : public nsDisplayItem {
public:
  nsDisplayButtonBoxShadowOuter(nsDisplayListBuilder* aBuilder,
                                nsButtonFrameRenderer* aRenderer)
    : nsDisplayItem(aBuilder, aRenderer->GetFrame()), mBFR(aRenderer) {
    MOZ_COUNT_CTOR(nsDisplayButtonBoxShadowOuter);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayButtonBoxShadowOuter() {
    MOZ_COUNT_DTOR(nsDisplayButtonBoxShadowOuter);
  }
#endif  
  
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx);
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap);
  NS_DISPLAY_DECL_NAME("ButtonBoxShadowOuter", TYPE_BUTTON_BOX_SHADOW_OUTER)
private:
  nsButtonFrameRenderer* mBFR;
};

nsRect
nsDisplayButtonBoxShadowOuter::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  *aSnap = false;
  return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
}

void
nsDisplayButtonBoxShadowOuter::Paint(nsDisplayListBuilder* aBuilder,
                                     nsRenderingContext* aCtx) {
  nsRect frameRect = nsRect(ToReferenceFrame(), mFrame->GetSize());

  nsRect buttonRect;
  mBFR->GetButtonRect(frameRect, buttonRect);

  nsCSSRendering::PaintBoxShadowOuter(mFrame->PresContext(), *aCtx, mFrame,
                                      buttonRect, mVisibleRect);
}

class nsDisplayButtonBorderBackground : public nsDisplayItem {
public:
  nsDisplayButtonBorderBackground(nsDisplayListBuilder* aBuilder,
                                  nsButtonFrameRenderer* aRenderer)
    : nsDisplayItem(aBuilder, aRenderer->GetFrame()), mBFR(aRenderer) {
    MOZ_COUNT_CTOR(nsDisplayButtonBorderBackground);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayButtonBorderBackground() {
    MOZ_COUNT_DTOR(nsDisplayButtonBorderBackground);
  }
#endif  
  
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) {
    aOutFrames->AppendElement(mFrame);
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx);
  NS_DISPLAY_DECL_NAME("ButtonBorderBackground", TYPE_BUTTON_BORDER_BACKGROUND)
private:
  nsButtonFrameRenderer* mBFR;
};

class nsDisplayButtonForeground : public nsDisplayItem {
public:
  nsDisplayButtonForeground(nsDisplayListBuilder* aBuilder,
                            nsButtonFrameRenderer* aRenderer)
    : nsDisplayItem(aBuilder, aRenderer->GetFrame()), mBFR(aRenderer) {
    MOZ_COUNT_CTOR(nsDisplayButtonForeground);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayButtonForeground() {
    MOZ_COUNT_DTOR(nsDisplayButtonForeground);
  }
#endif  

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx);
  NS_DISPLAY_DECL_NAME("ButtonForeground", TYPE_BUTTON_FOREGROUND)
private:
  nsButtonFrameRenderer* mBFR;
};

void nsDisplayButtonBorderBackground::Paint(nsDisplayListBuilder* aBuilder,
                                            nsRenderingContext* aCtx)
{
  NS_ASSERTION(mFrame, "No frame?");
  nsPresContext* pc = mFrame->PresContext();
  nsRect r = nsRect(ToReferenceFrame(), mFrame->GetSize());
  
  // draw the border and background inside the focus and outline borders
  mBFR->PaintBorderAndBackground(pc, *aCtx, mVisibleRect, r,
                                 aBuilder->GetBackgroundPaintFlags());
}

void nsDisplayButtonForeground::Paint(nsDisplayListBuilder* aBuilder,
                                      nsRenderingContext* aCtx)
{
  nsPresContext *presContext = mFrame->PresContext();
  const nsStyleDisplay *disp = mFrame->GetStyleDisplay();
  if (!mFrame->IsThemed(disp) ||
      !presContext->GetTheme()->ThemeDrawsFocusForWidget(presContext, mFrame, disp->mAppearance)) {
    // draw the focus and outline borders
    nsRect r = nsRect(ToReferenceFrame(), mFrame->GetSize());
    mBFR->PaintOutlineAndFocusBorders(presContext, *aCtx, mVisibleRect, r);
  }
}

nsresult
nsButtonFrameRenderer::DisplayButton(nsDisplayListBuilder* aBuilder,
                                     nsDisplayList* aBackground,
                                     nsDisplayList* aForeground)
{
  if (mFrame->GetStyleBorder()->mBoxShadow) {
    nsresult rv = aBackground->AppendNewToTop(new (aBuilder)
        nsDisplayButtonBoxShadowOuter(aBuilder, this));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsresult rv = aBackground->AppendNewToTop(new (aBuilder)
      nsDisplayButtonBorderBackground(aBuilder, this));
  NS_ENSURE_SUCCESS(rv, rv);

  return aForeground->AppendNewToTop(new (aBuilder)
      nsDisplayButtonForeground(aBuilder, this));
}

void
nsButtonFrameRenderer::PaintOutlineAndFocusBorders(nsPresContext* aPresContext,
          nsRenderingContext& aRenderingContext,
          const nsRect& aDirtyRect,
          const nsRect& aRect)
{
  // once we have all that we'll draw the focus if we have it. We will
  // need to draw 2 focuses, the inner and the outer. This is so we
  // can do any kind of look and feel. Some buttons have focus on the
  // outside like mac and motif. While others like windows have it
  // inside (dotted line).  Usually only one will be specifed. But I
  // guess you could have both if you wanted to.

  nsRect rect;

  if (mOuterFocusStyle) {
    // ---------- paint the outer focus border -------------

    GetButtonOuterFocusRect(aRect, rect);

    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, mFrame,
                                aDirtyRect, rect, mOuterFocusStyle);
  }

  if (mInnerFocusStyle) { 
    // ---------- paint the inner focus border -------------

    GetButtonInnerFocusRect(aRect, rect);

    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, mFrame,
                                aDirtyRect, rect, mInnerFocusStyle);
  }
}


void
nsButtonFrameRenderer::PaintBorderAndBackground(nsPresContext* aPresContext,
          nsRenderingContext& aRenderingContext,
          const nsRect& aDirtyRect,
          const nsRect& aRect,
          PRUint32 aBGFlags)

{
  // get the button rect this is inside the focus and outline rects
  nsRect buttonRect;
  GetButtonRect(aRect, buttonRect);

  nsStyleContext* context = mFrame->GetStyleContext();

  nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, mFrame,
                                  aDirtyRect, buttonRect, aBGFlags);
  nsCSSRendering::PaintBoxShadowInner(aPresContext, aRenderingContext,
                                      mFrame, buttonRect, aDirtyRect);
  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, mFrame,
                              aDirtyRect, buttonRect, context);
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


nsMargin
nsButtonFrameRenderer::GetButtonOuterFocusBorderAndPadding()
{
  nsMargin result(0,0,0,0);

  if (mOuterFocusStyle) {
    if (!mOuterFocusStyle->GetStylePadding()->GetPadding(result)) {
      NS_NOTYETIMPLEMENTED("percentage padding");
    }
    result += mOuterFocusStyle->GetStyleBorder()->GetActualBorder();
  }

  return result;
}

nsMargin
nsButtonFrameRenderer::GetButtonBorderAndPadding()
{
  return mFrame->GetUsedBorderAndPadding();
}

/**
 * Gets the size of the buttons border this is the union of the normal and disabled borders.
 */
nsMargin
nsButtonFrameRenderer::GetButtonInnerFocusMargin()
{
  nsMargin innerFocusMargin(0,0,0,0);

  if (mInnerFocusStyle) {
    const nsStyleMargin* margin = mInnerFocusStyle->GetStyleMargin();
    if (!margin->GetMargin(innerFocusMargin)) {
      NS_NOTYETIMPLEMENTED("percentage margin");
    }
  }

  return innerFocusMargin;
}

nsMargin
nsButtonFrameRenderer::GetButtonInnerFocusBorderAndPadding()
{
  nsMargin result(0,0,0,0);

  if (mInnerFocusStyle) {
    if (!mInnerFocusStyle->GetStylePadding()->GetPadding(result)) {
      NS_NOTYETIMPLEMENTED("percentage padding");
    }
    result += mInnerFocusStyle->GetStyleBorder()->GetActualBorder();
  }

  return result;
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
nsButtonFrameRenderer::ReResolveStyles(nsPresContext* aPresContext)
{
  // get all the styles
  nsStyleContext* context = mFrame->GetStyleContext();
  nsStyleSet *styleSet = aPresContext->StyleSet();

  // style for the inner such as a dotted line (Windows)
  mInnerFocusStyle =
    styleSet->ProbePseudoElementStyle(mFrame->GetContent()->AsElement(),
                                      nsCSSPseudoElements::ePseudo_mozFocusInner,
                                      context);

  // style for outer focus like a ridged border (MAC).
  mOuterFocusStyle =
    styleSet->ProbePseudoElementStyle(mFrame->GetContent()->AsElement(),
                                      nsCSSPseudoElements::ePseudo_mozFocusOuter,
                                      context);
}

nsStyleContext*
nsButtonFrameRenderer::GetStyleContext(PRInt32 aIndex) const
{
  switch (aIndex) {
  case NS_BUTTON_RENDERER_FOCUS_INNER_CONTEXT_INDEX:
    return mInnerFocusStyle;
  case NS_BUTTON_RENDERER_FOCUS_OUTER_CONTEXT_INDEX:
    return mOuterFocusStyle;
  default:
    return nsnull;
  }
}

void 
nsButtonFrameRenderer::SetStyleContext(PRInt32 aIndex, nsStyleContext* aStyleContext)
{
  switch (aIndex) {
  case NS_BUTTON_RENDERER_FOCUS_INNER_CONTEXT_INDEX:
    mInnerFocusStyle = aStyleContext;
    break;
  case NS_BUTTON_RENDERER_FOCUS_OUTER_CONTEXT_INDEX:
    mOuterFocusStyle = aStyleContext;
    break;
  }
}
