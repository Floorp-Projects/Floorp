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

#include "nsGfxCheckboxControlFrame.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsCSSRendering.h"
#include "nsRenderingContext.h"
#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif
#include "nsIServiceManager.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsDisplayList.h"
#include "nsCSSAnonBoxes.h"
#include "nsIDOMHTMLInputElement.h"

static void
PaintCheckMark(nsIFrame* aFrame,
               nsRenderingContext* aCtx,
               const nsRect& aDirtyRect,
               nsPoint aPt)
{
  nsRect rect(aPt, aFrame->GetSize());
  rect.Deflate(aFrame->GetUsedBorderAndPadding());

  // Points come from the coordinates on a 7X7 unit box centered at 0,0
  const PRInt32 checkPolygonX[] = { -3, -1,  3,  3, -1, -3 };
  const PRInt32 checkPolygonY[] = { -1,  1, -3, -1,  3,  1 };
  const PRInt32 checkNumPoints = sizeof(checkPolygonX) / sizeof(PRInt32);
  const PRInt32 checkSize      = 9; // 2 units of padding on either side
                                    // of the 7x7 unit checkmark

  // Scale the checkmark based on the smallest dimension
  nscoord paintScale = NS_MIN(rect.width, rect.height) / checkSize;
  nsPoint paintCenter(rect.x + rect.width  / 2,
                      rect.y + rect.height / 2);

  nsPoint paintPolygon[checkNumPoints];
  // Convert checkmark for screen rendering
  for (PRInt32 polyIndex = 0; polyIndex < checkNumPoints; polyIndex++) {
    paintPolygon[polyIndex] = paintCenter +
                              nsPoint(checkPolygonX[polyIndex] * paintScale,
                                      checkPolygonY[polyIndex] * paintScale);
  }

  aCtx->SetColor(aFrame->GetStyleColor()->mColor);
  aCtx->FillPolygon(paintPolygon, checkNumPoints);
}

static void
PaintIndeterminateMark(nsIFrame* aFrame,
                       nsRenderingContext* aCtx,
                       const nsRect& aDirtyRect,
                       nsPoint aPt)
{
  nsRect rect(aPt, aFrame->GetSize());
  rect.Deflate(aFrame->GetUsedBorderAndPadding());

  rect.y += (rect.height - rect.height/4) / 2;
  rect.height /= 4;

  aCtx->SetColor(aFrame->GetStyleColor()->mColor);
  aCtx->FillRect(rect);
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
already_AddRefed<nsAccessible>
nsGfxCheckboxControlFrame::CreateAccessible()
{
  nsAccessibilityService* accService = nsIPresShell::AccService();
  if (accService) {
    return accService->CreateHTMLCheckboxAccessible(mContent,
                                                    PresContext()->PresShell());
  }

  return nsnull;
}
#endif

//------------------------------------------------------------
NS_IMETHODIMP
nsGfxCheckboxControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                            const nsRect&           aDirtyRect,
                                            const nsDisplayListSet& aLists)
{
  nsresult rv = nsFormControlFrame::BuildDisplayList(aBuilder, aDirtyRect,
                                                     aLists);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Get current checked state through content model.
  if ((!IsChecked() && !IsIndeterminate()) || !IsVisibleForPainting(aBuilder))
    return NS_OK;   // we're not checked or not visible, nothing to paint.
    
  if (IsThemed())
    return NS_OK; // No need to paint the checkmark. The theme will do it.

  return aLists.Content()->AppendNewToTop(new (aBuilder)
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
