/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DoubleTapToZoom.h"

#include <algorithm>  // for std::min, std::max

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/Element.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLLIElement.h"
#include "nsIDOMHTMLQuoteElement.h"
#include "nsIDOMWindow.h"
#include "nsIFrame.h"
#include "nsIFrameInlines.h"
#include "nsIPresShell.h"
#include "nsLayoutUtils.h"
#include "nsStyleConsts.h"

namespace mozilla {
namespace layers {

// Returns the DOM element found at |aPoint|, interpreted as being relative to
// the root frame of |aShell|. If the point is inside a subdocument, returns
// an element inside the subdocument, rather than the subdocument element
// (and does so recursively).
// The implementation was adapted from nsDocument::ElementFromPoint(), with
// the notable exception that we don't pass nsLayoutUtils::IGNORE_CROSS_DOC
// to GetFrameForPoint(), so as to get the behaviour described above in the
// presence of subdocuments.
static already_AddRefed<dom::Element>
ElementFromPoint(const nsCOMPtr<nsIPresShell>& aShell,
                 const CSSPoint& aPoint)
{
  if (nsIFrame* rootFrame = aShell->GetRootFrame()) {
    if (nsIFrame* frame = nsLayoutUtils::GetFrameForPoint(rootFrame,
          CSSPoint::ToAppUnits(aPoint),
          nsLayoutUtils::IGNORE_PAINT_SUPPRESSION |
          nsLayoutUtils::IGNORE_ROOT_SCROLL_FRAME)) {
      while (frame && (!frame->GetContent() || frame->GetContent()->IsInAnonymousSubtree())) {
        frame = nsLayoutUtils::GetParentOrPlaceholderFor(frame);
      }
      nsIContent* content = frame->GetContent();
      if (content && !content->IsElement()) {
        content = content->GetParent();
      }
      if (content) {
        nsCOMPtr<dom::Element> result = content->AsElement();
        return result.forget();
      }
    }
  }
  return nullptr;
}

static bool
ShouldZoomToElement(const nsCOMPtr<dom::Element>& aElement) {
  if (nsIFrame* frame = aElement->GetPrimaryFrame()) {
    if (frame->GetDisplay() == NS_STYLE_DISPLAY_INLINE) {
      return false;
    }
  }
  if (aElement->IsAnyOfHTMLElements(nsGkAtoms::li, nsGkAtoms::q)) {
    return false;
  }
  return true;
}

// Calculate the bounding rect of |aElement|, relative to the origin
// of the scrolled content of |aRootScrollFrame|.
// The implementation of this calculation is adapted from
// Element::GetBoundingClientRect().
//
// Where the element is contained inside a scrollable subframe, the
// bounding rect is clipped to the bounds of the subframe.
static CSSRect
GetBoundingContentRect(const nsCOMPtr<dom::Element>& aElement,
                       const nsIScrollableFrame* aRootScrollFrame) {

  CSSRect result;
  if (nsIFrame* frame = aElement->GetPrimaryFrame()) {
    nsIFrame* relativeTo = aRootScrollFrame->GetScrolledFrame();
    result = CSSRect::FromAppUnits(
        nsLayoutUtils::GetAllInFlowRectsUnion(
            frame,
            relativeTo,
            nsLayoutUtils::RECTS_ACCOUNT_FOR_TRANSFORMS));

    // If the element is contained in a scrollable frame that is not
    // the root scroll frame, make sure to clip the result so that it is
    // not larger than the containing scrollable frame's bounds.
    nsIScrollableFrame* scrollFrame = nsLayoutUtils::GetNearestScrollableFrame(frame);
    if (scrollFrame && scrollFrame != aRootScrollFrame) {
      nsIFrame* subFrame = do_QueryFrame(scrollFrame);
      MOZ_ASSERT(subFrame);
      // Get the bounds of the scroll frame in the same coordinate space
      // as |result|.
      CSSRect subFrameRect = CSSRect::FromAppUnits(
          nsLayoutUtils::TransformFrameRectToAncestor(
              subFrame,
              subFrame->GetRectRelativeToSelf(),
              relativeTo));

      result = subFrameRect.Intersect(result);
    }
  }
  return result;
}

static bool
IsRectZoomedIn(const CSSRect& aRect, const CSSRect& aCompositedArea)
{
  // This functions checks to see if the area of the rect visible in the
  // composition bounds (i.e. the overlapArea variable below) is approximately
  // the max area of the rect we can show.
  CSSRect overlap = aCompositedArea.Intersect(aRect);
  float overlapArea = overlap.width * overlap.height;
  float availHeight = std::min(aRect.width * aCompositedArea.height / aCompositedArea.width,
                               aRect.height);
  float showing = overlapArea / (aRect.width * availHeight);
  float ratioW = aRect.width / aCompositedArea.width;
  float ratioH = aRect.height / aCompositedArea.height;

  return showing > 0.9 && (ratioW > 0.9 || ratioH > 0.9);
}

CSSRect
CalculateRectToZoomTo(const nsCOMPtr<nsIDocument>& aRootContentDocument,
                      const CSSPoint& aPoint)
{
  // Ensure the layout information we get is up-to-date.
  aRootContentDocument->FlushPendingNotifications(Flush_Layout);

  // An empty rect as return value is interpreted as "zoom out".
  const CSSRect zoomOut;

  nsCOMPtr<nsIPresShell> shell = aRootContentDocument->GetShell();
  if (!shell) {
    return zoomOut;
  }

  nsIScrollableFrame* rootScrollFrame = shell->GetRootScrollFrameAsScrollable();
  if (!rootScrollFrame) {
    return zoomOut;
  }

  nsCOMPtr<dom::Element> element = ElementFromPoint(shell, aPoint);
  if (!element) {
    return zoomOut;
  }

  while (element && !ShouldZoomToElement(element)) {
    element = element->GetParentElement();
  }

  if (!element) {
    return zoomOut;
  }

  FrameMetrics metrics = nsLayoutUtils::CalculateBasicFrameMetrics(rootScrollFrame);
  CSSRect compositedArea(metrics.GetScrollOffset(), metrics.CalculateCompositedSizeInCssPixels());
  const CSSCoord margin = 15;
  CSSRect rect = GetBoundingContentRect(element, rootScrollFrame);

  // If the element is taller than the visible area of the page scale
  // the height of the |rect| so that it has the same aspect ratio as
  // the root frame.  The clipped |rect| is centered on the y value of
  // the touch point. This allows tall narrow elements to be zoomed.
  if (!rect.IsEmpty() && compositedArea.width > 0.0f) {
    const float widthRatio = rect.width / compositedArea.width;
    float targetHeight = compositedArea.height * widthRatio;
    if (widthRatio < 0.9 && targetHeight < rect.height) {
      const CSSPoint scrollPoint = CSSPoint::FromAppUnits(rootScrollFrame->GetScrollPosition());
      float newY = aPoint.y + scrollPoint.y - (targetHeight * 0.5f);
      if ((newY + targetHeight) > (rect.y + rect.height)) {
        rect.y += rect.height - targetHeight;
      } else if (newY > rect.y) {
        rect.y = newY;
      }
      rect.height = targetHeight;
    }
  }

  rect = CSSRect(std::max(metrics.GetScrollableRect().x, rect.x - margin),
                 rect.y,
                 rect.width + 2 * margin,
                 rect.height);
  // Constrict the rect to the screen's right edge
  rect.width = std::min(rect.width, metrics.GetScrollableRect().XMost() - rect.x);

  // If the rect is already taking up most of the visible area and is
  // stretching the width of the page, then we want to zoom out instead.
  if (IsRectZoomedIn(rect, compositedArea)) {
    return zoomOut;
  }

  CSSRect rounded(rect);
  rounded.Round();

  // If the block we're zooming to is really tall, and the user double-tapped
  // more than a screenful of height from the top of it, then adjust the
  // y-coordinate so that we center the actual point the user double-tapped
  // upon. This prevents flying to the top of the page when double-tapping
  // to zoom in (bug 761721). The 1.2 multiplier is just a little fuzz to
  // compensate for 'rect' including horizontal margins but not vertical ones.
  CSSCoord cssTapY = metrics.GetScrollOffset().y + aPoint.y;
  if ((rect.height > rounded.height) && (cssTapY > rounded.y + (rounded.height * 1.2))) {
    rounded.y = cssTapY - (rounded.height / 2);
  }

  return rounded;
}

}
}
