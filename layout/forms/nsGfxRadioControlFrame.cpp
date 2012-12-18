/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGfxRadioControlFrame.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsCSSRendering.h"
#include "nsRenderingContext.h"
#include "nsIServiceManager.h"
#include "nsITheme.h"
#include "nsDisplayList.h"
#include "nsCSSAnonBoxes.h"

using namespace mozilla;

nsIFrame*
NS_NewGfxRadioControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsGfxRadioControlFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsGfxRadioControlFrame)

nsGfxRadioControlFrame::nsGfxRadioControlFrame(nsStyleContext* aContext):
  nsFormControlFrame(aContext)
{
}

nsGfxRadioControlFrame::~nsGfxRadioControlFrame()
{
}

#ifdef ACCESSIBILITY
a11y::AccType
nsGfxRadioControlFrame::AccessibleType()
{
  return a11y::eHTMLRadioButtonType;
}
#endif

//--------------------------------------------------------------
// Draw the dot for a non-native radio button in the checked state.
static void
PaintCheckedRadioButton(nsIFrame* aFrame,
                        nsRenderingContext* aCtx,
                        const nsRect& aDirtyRect,
                        nsPoint aPt)
{
  // The dot is an ellipse 2px on all sides smaller than the content-box,
  // drawn in the foreground color.
  nsRect rect(aPt, aFrame->GetSize());
  rect.Deflate(aFrame->GetUsedBorderAndPadding());
  rect.Deflate(nsPresContext::CSSPixelsToAppUnits(2),
               nsPresContext::CSSPixelsToAppUnits(2));

  aCtx->SetColor(aFrame->GetStyleColor()->mColor);
  aCtx->FillEllipse(rect);
}

NS_IMETHODIMP
nsGfxRadioControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                         const nsRect&           aDirtyRect,
                                         const nsDisplayListSet& aLists)
{
  nsresult rv = nsFormControlFrame::BuildDisplayList(aBuilder, aDirtyRect,
                                                     aLists);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!IsVisibleForPainting(aBuilder))
    return NS_OK;
  
  if (IsThemed())
    return NS_OK; // The theme will paint the check, if any.

  bool checked = true;
  GetCurrentCheckState(&checked); // Get check state from the content model
  if (!checked)
    return NS_OK;
    
  return aLists.Content()->AppendNewToTop(new (aBuilder)
    nsDisplayGeneric(aBuilder, this, PaintCheckedRadioButton,
                     "CheckedRadioButton",
                     nsDisplayItem::TYPE_CHECKED_RADIOBUTTON));
}
