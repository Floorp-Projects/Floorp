/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DoubleTapToZoom.h"

#include <algorithm>  // for std::min, std::max

#include "mozilla/PresShell.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/Element.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "mozilla/dom/Document.h"
#include "nsIFrame.h"
#include "nsIFrameInlines.h"
#include "nsIScrollableFrame.h"
#include "nsTableCellFrame.h"
#include "nsLayoutUtils.h"
#include "nsStyleConsts.h"
#include "mozilla/ViewportUtils.h"
#include "mozilla/EventListenerManager.h"

namespace mozilla {
namespace layers {

namespace {

using FrameForPointOption = nsLayoutUtils::FrameForPointOption;

static bool IsGeneratedContent(nsIContent* aContent) {
  // We exclude marks because making them double tap targets does not seem
  // desirable.
  return aContent->IsGeneratedContentContainerForBefore() ||
         aContent->IsGeneratedContentContainerForAfter();
}

// Returns the DOM element found at |aPoint|, interpreted as being relative to
// the root frame of |aPresShell| in visual coordinates. If the point is inside
// a subdocument, returns an element inside the subdocument, rather than the
// subdocument element (and does so recursively). The implementation was adapted
// from DocumentOrShadowRoot::ElementFromPoint(), with the notable exception
// that we don't pass nsLayoutUtils::IGNORE_CROSS_DOC to GetFrameForPoint(), so
// as to get the behaviour described above in the presence of subdocuments.
static already_AddRefed<dom::Element> ElementFromPoint(
    const RefPtr<PresShell>& aPresShell, const CSSPoint& aPoint) {
  nsIFrame* rootFrame = aPresShell->GetRootFrame();
  if (!rootFrame) {
    return nullptr;
  }
  nsIFrame* frame = nsLayoutUtils::GetFrameForPoint(
      RelativeTo{rootFrame, ViewportType::Visual}, CSSPoint::ToAppUnits(aPoint),
      {{FrameForPointOption::IgnorePaintSuppression}});
  while (frame && (!frame->GetContent() ||
                   (frame->GetContent()->IsInNativeAnonymousSubtree() &&
                    !IsGeneratedContent(frame->GetContent())))) {
    frame = nsLayoutUtils::GetParentOrPlaceholderFor(frame);
  }
  if (!frame) {
    return nullptr;
  }
  // FIXME(emilio): This should probably use the flattened tree, GetParent() is
  // not guaranteed to be an element in presence of shadow DOM.
  nsIContent* content = frame->GetContent();
  if (!content) {
    return nullptr;
  }
  if (dom::Element* element = content->GetAsElementOrParentElement()) {
    return do_AddRef(element);
  }
  return nullptr;
}

// Get table cell from element, parent or grand parent.
static dom::Element* GetNearbyTableCell(
    const nsCOMPtr<dom::Element>& aElement) {
  nsTableCellFrame* tableCell = do_QueryFrame(aElement->GetPrimaryFrame());
  if (tableCell) {
    return aElement.get();
  }
  if (dom::Element* parent = aElement->GetFlattenedTreeParentElement()) {
    nsTableCellFrame* tableCell = do_QueryFrame(parent->GetPrimaryFrame());
    if (tableCell) {
      return parent;
    }
    if (dom::Element* grandParent = parent->GetFlattenedTreeParentElement()) {
      tableCell = do_QueryFrame(grandParent->GetPrimaryFrame());
      if (tableCell) {
        return grandParent;
      }
    }
  }
  return nullptr;
}

static bool ShouldZoomToElement(
    const nsCOMPtr<dom::Element>& aElement,
    const RefPtr<dom::Document>& aRootContentDocument,
    nsIScrollableFrame* aRootScrollFrame, const FrameMetrics& aMetrics) {
  if (nsIFrame* frame = aElement->GetPrimaryFrame()) {
    if (frame->StyleDisplay()->IsInlineFlow() &&
        // Replaced elements are suitable zoom targets because they act like
        // inline-blocks instead of inline. (textarea's are the specific reason
        // we do this)
        !frame->IsFrameOfType(nsIFrame::eReplaced)) {
      return false;
    }
  }
  // Trying to zoom to the html element will just end up scrolling to the start
  // of the document, return false and we'll run out of elements and just
  // zoomout (without scrolling to the start).
  if (aElement->OwnerDoc() == aRootContentDocument &&
      aElement->IsHTMLElement(nsGkAtoms::html)) {
    return false;
  }
  if (aElement->IsAnyOfHTMLElements(nsGkAtoms::li, nsGkAtoms::q)) {
    return false;
  }

  // Ignore elements who are table cells or their parents are table cells, and
  // they take up less than 30% of page rect width because they are likely cells
  // in data tables (as opposed to tables used for layout purposes), and we
  // don't want to zoom to them. This heuristic is quite naive and leaves a lot
  // to be desired.
  if (dom::Element* tableCell = GetNearbyTableCell(aElement)) {
    CSSRect rect =
        nsLayoutUtils::GetBoundingContentRect(tableCell, aRootScrollFrame);
    if (rect.width < 0.3 * aMetrics.GetScrollableRect().width) {
      return false;
    }
  }

  return true;
}

// Calculates if zooming to aRect would have almost the same zoom level as
// aCompositedArea currently has. If so we would want to zoom out instead.
static bool RectHasAlmostSameZoomLevel(const CSSRect& aRect,
                                       const CSSRect& aCompositedArea) {
  // This functions checks to see if the area of the rect visible in the
  // composition bounds (i.e. the overlapArea variable below) is approximately
  // the max area of the rect we can show.

  // AsyncPanZoomController::ZoomToRect will adjust the zoom and scroll offset
  // so that the zoom to rect fills the composited area. If after adjusting the
  // scroll offset _only_ the rect would fill the composited area we want to
  // zoom out (we don't want to _just_ scroll, we want to do some amount of
  // zooming, either in or out it doesn't matter which). So translate both rects
  // to the same origin and then compute their overlap, which is what the
  // following calculation does.

  float overlapArea = std::min(aRect.width, aCompositedArea.width) *
                      std::min(aRect.height, aCompositedArea.height);
  float availHeight = std::min(
      aRect.Width() * aCompositedArea.Height() / aCompositedArea.Width(),
      aRect.Height());
  float showing = overlapArea / (aRect.Width() * availHeight);
  float ratioW = aRect.Width() / aCompositedArea.Width();
  float ratioH = aRect.Height() / aCompositedArea.Height();

  return showing > 0.9 && (ratioW > 0.9 || ratioH > 0.9);
}

}  // namespace

static CSSRect AddHMargin(const CSSRect& aRect, const CSSCoord& aMargin,
                          const FrameMetrics& aMetrics) {
  CSSRect rect =
      CSSRect(std::max(aMetrics.GetScrollableRect().X(), aRect.X() - aMargin),
              aRect.Y(), aRect.Width() + 2 * aMargin, aRect.Height());
  // Constrict the rect to the screen's right edge
  rect.SetWidth(
      std::min(rect.Width(), aMetrics.GetScrollableRect().XMost() - rect.X()));
  return rect;
}

static CSSRect AddVMargin(const CSSRect& aRect, const CSSCoord& aMargin,
                          const FrameMetrics& aMetrics) {
  CSSRect rect =
      CSSRect(aRect.X(),
              std::max(aMetrics.GetScrollableRect().Y(), aRect.Y() - aMargin),
              aRect.Width(), aRect.Height() + 2 * aMargin);
  // Constrict the rect to the screen's bottom edge
  rect.SetHeight(
      std::min(rect.Height(), aMetrics.GetScrollableRect().YMost() - rect.Y()));
  return rect;
}

static bool IsReplacedElement(const nsCOMPtr<dom::Element>& aElement) {
  if (nsIFrame* frame = aElement->GetPrimaryFrame()) {
    if (frame->IsFrameOfType(nsIFrame::eReplaced)) {
      return true;
    }
  }
  return false;
}

static bool HasNonPassiveWheelListenerOnAncestor(nsIContent* aContent) {
  for (nsIContent* content = aContent; content;
       content = content->GetFlattenedTreeParent()) {
    EventListenerManager* elm = content->GetExistingListenerManager();
    if (elm && elm->HasNonPassiveWheelListener()) {
      return true;
    }
  }
  return false;
}

ZoomTarget CalculateRectToZoomTo(
    const RefPtr<dom::Document>& aRootContentDocument, const CSSPoint& aPoint) {
  // Ensure the layout information we get is up-to-date.
  aRootContentDocument->FlushPendingNotifications(FlushType::Layout);

  // An empty rect as return value is interpreted as "zoom out".
  const CSSRect zoomOut;

  RefPtr<PresShell> presShell = aRootContentDocument->GetPresShell();
  if (!presShell) {
    return ZoomTarget{zoomOut, CantZoomOutBehavior::ZoomIn};
  }

  nsIScrollableFrame* rootScrollFrame =
      presShell->GetRootScrollFrameAsScrollable();
  if (!rootScrollFrame) {
    return ZoomTarget{zoomOut, CantZoomOutBehavior::ZoomIn};
  }

  CSSPoint documentRelativePoint =
      CSSPoint::FromAppUnits(ViewportUtils::VisualToLayout(
          CSSPoint::ToAppUnits(aPoint), presShell)) +
      CSSPoint::FromAppUnits(rootScrollFrame->GetScrollPosition());

  nsCOMPtr<dom::Element> element = ElementFromPoint(presShell, aPoint);
  if (!element) {
    return ZoomTarget{zoomOut, CantZoomOutBehavior::ZoomIn, Nothing(),
                      Some(documentRelativePoint)};
  }

  CantZoomOutBehavior cantZoomOutBehavior =
      HasNonPassiveWheelListenerOnAncestor(element)
          ? CantZoomOutBehavior::Nothing
          : CantZoomOutBehavior::ZoomIn;

  FrameMetrics metrics =
      nsLayoutUtils::CalculateBasicFrameMetrics(rootScrollFrame);

  while (element && !ShouldZoomToElement(element, aRootContentDocument,
                                         rootScrollFrame, metrics)) {
    element = element->GetFlattenedTreeParentElement();
  }

  if (!element) {
    return ZoomTarget{zoomOut, cantZoomOutBehavior, Nothing(),
                      Some(documentRelativePoint)};
  }

  CSSPoint visualScrollOffset = metrics.GetVisualScrollOffset();
  CSSRect compositedArea(visualScrollOffset,
                         metrics.CalculateCompositedSizeInCssPixels());
  Maybe<CSSRect> nearestScrollClip;
  CSSRect rect = nsLayoutUtils::GetBoundingContentRect(element, rootScrollFrame,
                                                       &nearestScrollClip);

  // In some cases, like overflow: visible and overflowing content, the bounding
  // client rect of the targeted element won't contain the point the user double
  // tapped on. In that case we use the scrollable overflow rect if it contains
  // the user point.
  if (!rect.Contains(documentRelativePoint)) {
    if (nsIFrame* scrolledFrame = rootScrollFrame->GetScrolledFrame()) {
      if (nsIFrame* f = element->GetPrimaryFrame()) {
        nsRect overflowRect = f->ScrollableOverflowRect();
        nsLayoutUtils::TransformResult res =
            nsLayoutUtils::TransformRect(f, scrolledFrame, overflowRect);
        MOZ_ASSERT(res == nsLayoutUtils::TRANSFORM_SUCCEEDED ||
                   res == nsLayoutUtils::NONINVERTIBLE_TRANSFORM);
        if (res == nsLayoutUtils::TRANSFORM_SUCCEEDED) {
          CSSRect overflowRectCSS = CSSRect::FromAppUnits(overflowRect);
          if (nearestScrollClip.isSome()) {
            overflowRectCSS = nearestScrollClip->Intersect(overflowRectCSS);
          }
          if (overflowRectCSS.Contains(documentRelativePoint)) {
            rect = overflowRectCSS;
          }
        }
      }
    }
  }

  CSSRect elementBoundingRect = rect;

  // Generally we zoom to the width of some element, but sometimes we zoom to
  // the height. We set this to true when that happens so that we can add a
  // vertical margin to the rect, otherwise it looks weird.
  bool heightConstrained = false;

  // If the element is taller than the visible area of the page scale
  // the height of the |rect| so that it has the same aspect ratio as
  // the root frame.  The clipped |rect| is centered on the y value of
  // the touch point. This allows tall narrow elements to be zoomed.
  if (!rect.IsEmpty() && compositedArea.Width() > 0.0f &&
      compositedArea.Height() > 0.0f) {
    // Calculate the height of the rect if it had the same aspect ratio as
    // compositedArea.
    const float widthRatio = rect.Width() / compositedArea.Width();
    float targetHeight = compositedArea.Height() * widthRatio;

    // We don't want to cut off the top or bottoms of replaced elements that are
    // square or wider in aspect ratio.

    // If it's a replaced element and we would otherwise trim it's height below
    if (IsReplacedElement(element) && targetHeight < rect.Height() &&
        // If the target rect is at most 1.1x away from being square or wider
        // aspect ratio
        rect.Height() < 1.1 * rect.Width() &&
        // and our compositedArea is wider than it is tall
        compositedArea.Width() >= compositedArea.Height()) {
      heightConstrained = true;
      // Expand the width of the rect so that it fills compositedArea so that if
      // we are already zoomed to this element then the IsRectZoomedIn call
      // below returns true so that we zoom out. This won't change what we
      // actually zoom to as we are just making the rect the same aspect ratio
      // as compositedArea.
      float targetWidth =
          rect.Height() * compositedArea.Width() / compositedArea.Height();
      MOZ_ASSERT(targetWidth > rect.Width());
      if (targetWidth > rect.Width()) {
        rect.x -= (targetWidth - rect.Width()) / 2;
        rect.SetWidth(targetWidth);
        // keep elementBoundingRect containing rect
        elementBoundingRect = rect;
      }

    } else if (targetHeight < rect.Height()) {
      // Trim the height so that the target rect has the same aspect ratio as
      // compositedArea, centering it around the user tap point.
      float newY = documentRelativePoint.y - (targetHeight * 0.5f);
      if ((newY + targetHeight) > rect.YMost()) {
        rect.MoveByY(rect.Height() - targetHeight);
      } else if (newY > rect.Y()) {
        rect.MoveToY(newY);
      }
      rect.SetHeight(targetHeight);
    }
  }

  const CSSCoord margin = 15;
  rect = AddHMargin(rect, margin, metrics);

  if (heightConstrained) {
    rect = AddVMargin(rect, margin, metrics);
  }

  // If the rect is already taking up most of the visible area and is
  // stretching the width of the page, then we want to zoom out instead.
  if (RectHasAlmostSameZoomLevel(rect, compositedArea)) {
    return ZoomTarget{zoomOut, cantZoomOutBehavior, Nothing(),
                      Some(documentRelativePoint)};
  }

  elementBoundingRect = AddHMargin(elementBoundingRect, margin, metrics);

  // Unlike rect, elementBoundingRect is the full height of the element we are
  // zooming to. If we zoom to it without a margin it can look a weird, so give
  // it a vertical margin.
  elementBoundingRect = AddVMargin(elementBoundingRect, margin, metrics);

  rect.Round();
  elementBoundingRect.Round();
  return ZoomTarget{rect, cantZoomOutBehavior, Some(elementBoundingRect),
                    Some(documentRelativePoint)};
}

}  // namespace layers
}  // namespace mozilla
