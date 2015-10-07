/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGfxCheckboxControlFrame.h"

#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsLayoutUtils.h"
#include "nsRenderingContext.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsDisplayList.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::gfx;

static void
PaintCheckMark(nsIFrame* aFrame,
               nsRenderingContext* aCtx,
               const nsRect& aDirtyRect,
               nsPoint aPt)
{
  nsRect rect(aPt, aFrame->GetSize());
  rect.Deflate(aFrame->GetUsedBorderAndPadding());

  // Points come from the coordinates on a 7X7 unit box centered at 0,0
  const int32_t checkPolygonX[] = { -3, -1,  3,  3, -1, -3 };
  const int32_t checkPolygonY[] = { -1,  1, -3, -1,  3,  1 };
  const int32_t checkNumPoints = sizeof(checkPolygonX) / sizeof(int32_t);
  const int32_t checkSize      = 9; // 2 units of padding on either side
                                    // of the 7x7 unit checkmark

  // Scale the checkmark based on the smallest dimension
  nscoord paintScale = std::min(rect.width, rect.height) / checkSize;
  nsPoint paintCenter(rect.x + rect.width  / 2,
                      rect.y + rect.height / 2);

  DrawTarget* drawTarget = aCtx->GetDrawTarget();
  RefPtr<PathBuilder> builder = drawTarget->CreatePathBuilder();
  nsPoint p = paintCenter + nsPoint(checkPolygonX[0] * paintScale,
                                    checkPolygonY[0] * paintScale);

  int32_t appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  builder->MoveTo(NSPointToPoint(p, appUnitsPerDevPixel));
  for (int32_t polyIndex = 1; polyIndex < checkNumPoints; polyIndex++) {
    p = paintCenter + nsPoint(checkPolygonX[polyIndex] * paintScale,
                              checkPolygonY[polyIndex] * paintScale);
    builder->LineTo(NSPointToPoint(p, appUnitsPerDevPixel));
  }
  RefPtr<Path> path = builder->Finish();
  drawTarget->Fill(path,
                   ColorPattern(ToDeviceColor(aFrame->StyleColor()->mColor)));
}

static void
PaintIndeterminateMark(nsIFrame* aFrame,
                       nsRenderingContext* aCtx,
                       const nsRect& aDirtyRect,
                       nsPoint aPt)
{
  DrawTarget* drawTarget = aCtx->GetDrawTarget();
  int32_t appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();

  nsRect rect(aPt, aFrame->GetSize());
  rect.Deflate(aFrame->GetUsedBorderAndPadding());
  rect.y += (rect.height - rect.height/4) / 2;
  rect.height /= 4;

  Rect devPxRect = NSRectToSnappedRect(rect, appUnitsPerDevPixel, *drawTarget);

  drawTarget->FillRect(devPxRect,
                    ColorPattern(ToDeviceColor(aFrame->StyleColor()->mColor)));
}

//------------------------------------------------------------
nsIFrame*
NS_NewGfxCheckboxControlFrame(nsIPresShell* aPresShell,
                              nsStyleContext* aContext)
{
  return new (aPresShell) nsGfxCheckboxControlFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsGfxCheckboxControlFrame)


//------------------------------------------------------------
// Initialize GFX-rendered state
nsGfxCheckboxControlFrame::nsGfxCheckboxControlFrame(nsStyleContext* aContext)
: nsFormControlFrame(aContext)
{
}

nsGfxCheckboxControlFrame::~nsGfxCheckboxControlFrame()
{
}

#ifdef ACCESSIBILITY
a11y::AccType
nsGfxCheckboxControlFrame::AccessibleType()
{
  return a11y::eHTMLCheckboxType;
}
#endif

//------------------------------------------------------------
void
nsGfxCheckboxControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                            const nsRect&           aDirtyRect,
                                            const nsDisplayListSet& aLists)
{
  nsFormControlFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
  
  // Get current checked state through content model.
  if ((!IsChecked() && !IsIndeterminate()) || !IsVisibleForPainting(aBuilder))
    return;   // we're not checked or not visible, nothing to paint.
    
  if (IsThemed())
    return; // No need to paint the checkmark. The theme will do it.

  aLists.Content()->AppendNewToTop(new (aBuilder)
    nsDisplayGeneric(aBuilder, this,
                     IsIndeterminate()
                     ? PaintIndeterminateMark : PaintCheckMark,
                     "CheckedCheckbox",
                     nsDisplayItem::TYPE_CHECKED_CHECKBOX));
}

//------------------------------------------------------------
bool
nsGfxCheckboxControlFrame::IsChecked()
{
  nsCOMPtr<nsIDOMHTMLInputElement> elem(do_QueryInterface(mContent));
  bool retval = false;
  elem->GetChecked(&retval);
  return retval;
}

bool
nsGfxCheckboxControlFrame::IsIndeterminate()
{
  nsCOMPtr<nsIDOMHTMLInputElement> elem(do_QueryInterface(mContent));
  bool retval = false;
  elem->GetIndeterminate(&retval);
  return retval;
}
