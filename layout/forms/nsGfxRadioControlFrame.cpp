/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGfxRadioControlFrame.h"

#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"

using namespace mozilla;
using namespace mozilla::gfx;

nsIFrame*
NS_NewGfxRadioControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsGfxRadioControlFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsGfxRadioControlFrame)

nsGfxRadioControlFrame::nsGfxRadioControlFrame(nsStyleContext* aContext):
  nsFormControlFrame(aContext, kClassID)
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

#ifdef MOZ_WIDGET_ANDROID

//--------------------------------------------------------------
// Draw the dot for a non-native radio button in the checked state.
static void
PaintCheckedRadioButton(nsIFrame* aFrame,
                        DrawTarget* aDrawTarget,
                        const nsRect& aDirtyRect,
                        nsPoint aPt)
{
  // The dot is an ellipse 2px on all sides smaller than the content-box,
  // drawn in the foreground color.
  nsRect rect(aPt, aFrame->GetSize());
  rect.Deflate(aFrame->GetUsedBorderAndPadding());
  rect.Deflate(nsPresContext::CSSPixelsToAppUnits(2),
               nsPresContext::CSSPixelsToAppUnits(2));

  Rect devPxRect =
    ToRect(nsLayoutUtils::RectToGfxRect(rect,
                                        aFrame->PresContext()->AppUnitsPerDevPixel()));

  ColorPattern color(ToDeviceColor(aFrame->StyleColor()->mColor));

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  AppendEllipseToPath(builder, devPxRect.Center(), devPxRect.Size());
  RefPtr<Path> ellipse = builder->Finish();
  aDrawTarget->Fill(ellipse, color);
}

void
nsGfxRadioControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                         const nsRect&           aDirtyRect,
                                         const nsDisplayListSet& aLists)
{
  nsFormControlFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);

  if (!IsVisibleForPainting(aBuilder))
    return;

  if (IsThemed())
    return; // The theme will paint the check, if any.

  bool checked = true;
  GetCurrentCheckState(&checked); // Get check state from the content model
  if (!checked)
    return;

  aLists.Content()->AppendNewToTop(new (aBuilder)
    nsDisplayGeneric(aBuilder, this, PaintCheckedRadioButton,
                     "CheckedRadioButton",
                     nsDisplayItem::TYPE_CHECKED_RADIOBUTTON));
}

#endif
