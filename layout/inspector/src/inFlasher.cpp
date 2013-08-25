/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "inFlasher.h"
#include "inLayoutUtils.h"

#include "nsIDOMElement.h"
#include "nsIServiceManager.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIWidget.h"
#include "nsReadableUtils.h"
#include "nsRenderingContext.h"

#include "prprf.h"

///////////////////////////////////////////////////////////////////////////////

inFlasher::inFlasher() :
  mColor(NS_RGB(0,0,0)),
  mThickness(0),
  mInvert(false)
{
}

inFlasher::~inFlasher()
{
}

NS_IMPL_ISUPPORTS1(inFlasher, inIFlasher)

///////////////////////////////////////////////////////////////////////////////
// inIFlasher

NS_IMETHODIMP
inFlasher::GetColor(nsAString& aColor)
{
  // Copied from nsGenericHTMLElement::ColorToString()
  char buf[10];
  PR_snprintf(buf, sizeof(buf), "#%02x%02x%02x",
              NS_GET_R(mColor), NS_GET_G(mColor), NS_GET_B(mColor));
  CopyASCIItoUTF16(buf, aColor);

  return NS_OK;
}

NS_IMETHODIMP
inFlasher::SetColor(const nsAString& aColor)
{
  NS_ENSURE_FALSE(aColor.IsEmpty(), NS_ERROR_ILLEGAL_VALUE);

  nsAutoString colorStr;
  colorStr.Assign(aColor);

  if (colorStr.CharAt(0) != '#') {
    if (NS_ColorNameToRGB(colorStr, &mColor)) {
      return NS_OK;
    }
  }
  else {
    colorStr.Cut(0, 1);
    if (NS_HexToRGB(colorStr, &mColor)) {
      return NS_OK;
    }
  }

  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
inFlasher::GetThickness(uint16_t *aThickness)
{
  NS_PRECONDITION(aThickness, "Null pointer");
  *aThickness = mThickness;
  return NS_OK;
}

NS_IMETHODIMP
inFlasher::SetThickness(uint16_t aThickness)
{
  mThickness = aThickness;
  return NS_OK;
}

NS_IMETHODIMP
inFlasher::GetInvert(bool *aInvert)
{
  NS_PRECONDITION(aInvert, "Null pointer");
  *aInvert = mInvert;
  return NS_OK;
}

NS_IMETHODIMP
inFlasher::SetInvert(bool aInvert)
{
  mInvert = aInvert;
  return NS_OK;
}

NS_IMETHODIMP
inFlasher::RepaintElement(nsIDOMElement* aElement)
{
  NS_ENSURE_ARG_POINTER(aElement);
  nsIFrame* frame = inLayoutUtils::GetFrameFor(aElement);
  if (!frame) return NS_OK;

  frame->InvalidateFrame();

  return NS_OK;
}

NS_IMETHODIMP
inFlasher::DrawElementOutline(nsIDOMElement* aElement)
{
  NS_ENSURE_ARG_POINTER(aElement);
  nsCOMPtr<nsIDOMWindow> window = inLayoutUtils::GetWindowFor(aElement);
  if (!window) return NS_OK;
  nsCOMPtr<nsIPresShell> presShell = inLayoutUtils::GetPresShellFor(window);
  if (!presShell) return NS_OK;

  nsIFrame* frame = inLayoutUtils::GetFrameFor(aElement);

  bool isFirstFrame = true;

  while (frame) {
    nsPoint offset;
    nsIWidget* widget = frame->GetNearestWidget(offset);
    if (widget) {
      nsRefPtr<nsRenderingContext> rcontext = new nsRenderingContext();
      rcontext->Init(frame->PresContext()->DeviceContext(),
                     widget->GetThebesSurface());

      nsRect rect(offset, frame->GetSize());
      if (mInvert) {
        rcontext->InvertRect(rect);
      }

      bool isLastFrame = frame->GetNextContinuation() == nullptr;
      DrawOutline(rect.x, rect.y, rect.width, rect.height, rcontext,
                  isFirstFrame, isLastFrame);
      isFirstFrame = false;
    }
    frame = frame->GetNextContinuation();
  }

  return NS_OK;
}

NS_IMETHODIMP
inFlasher::ScrollElementIntoView(nsIDOMElement *aElement)
{
  NS_ENSURE_ARG_POINTER(aElement);
  nsCOMPtr<nsIDOMWindow> window = inLayoutUtils::GetWindowFor(aElement);
  if (!window) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> presShell = inLayoutUtils::GetPresShellFor(window);
  if (!presShell) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  presShell->ScrollContentIntoView(content,
                                   nsIPresShell::ScrollAxis(),
                                   nsIPresShell::ScrollAxis(),
                                   nsIPresShell::SCROLL_OVERFLOW_HIDDEN);

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// inFlasher

void
inFlasher::DrawOutline(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                       nsRenderingContext* aRenderContext,
                       bool aDrawBegin, bool aDrawEnd)
{
  aRenderContext->SetColor(mColor);

  DrawLine(aX, aY, aWidth, DIR_HORIZONTAL, BOUND_OUTER, aRenderContext);
  if (aDrawBegin) {
    DrawLine(aX, aY, aHeight, DIR_VERTICAL, BOUND_OUTER, aRenderContext);
  }
  DrawLine(aX, aY+aHeight, aWidth, DIR_HORIZONTAL, BOUND_INNER, aRenderContext);
  if (aDrawEnd) {
    DrawLine(aX+aWidth, aY, aHeight, DIR_VERTICAL, BOUND_INNER, aRenderContext);
  }
}

void
inFlasher::DrawLine(nscoord aX, nscoord aY, nscoord aLength,
                    bool aDir, bool aBounds,
                    nsRenderingContext* aRenderContext)
{
  nscoord thickTwips = nsPresContext::CSSPixelsToAppUnits(mThickness);
  if (aDir) { // horizontal
    aRenderContext->FillRect(aX, aY+(aBounds?0:-thickTwips), aLength, thickTwips);
  } else { // vertical
    aRenderContext->FillRect(aX+(aBounds?0:-thickTwips), aY, thickTwips, aLength);
  }
}
