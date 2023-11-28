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
#include "mozilla/dom/EffectsInfo.h"
#include "mozilla/dom/BrowserChild.h"
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
#include "mozilla/layers/APZUtils.h"

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

// A utility function returns the given |aElement| rectangle relative to the top
// level content document coordinates.
static CSSRect GetBoundingContentRect(
    const dom::Element* aElement,
    const RefPtr<dom::Document>& aInProcessRootContentDocument,
    const nsIScrollableFrame* aRootScrollFrame,
    const DoubleTapToZoomMetrics& aMetrics,
    mozilla::Maybe<CSSRect>* aOutNearestScrollClip = nullptr) {
  CSSRect result = nsLayoutUtils::GetBoundingContentRect(
      aElement, aRootScrollFrame, aOutNearestScrollClip);
  if (aInProcessRootContentDocument->IsTopLevelContentDocument()) {
    return result;
  }

  nsIFrame* frame = aElement->GetPrimaryFrame();
  if (!frame) {
    return CSSRect();
  }

  // If the nearest scroll frame is |aRootScrollFrame|,
  // nsLayoutUtils::GetBoundingContentRect doesn't set |aOutNearestScrollClip|,
  // thus in the cases of OOP iframs, we need to use the visible rect of the
  // iframe as the nearest scroll clip.
  if (aOutNearestScrollClip && aOutNearestScrollClip->isNothing()) {
    if (dom::BrowserChild* browserChild =
            dom::BrowserChild::GetFrom(frame->PresShell())) {
      const dom::EffectsInfo& effectsInfo = browserChild->GetEffectsInfo();
      if (effectsInfo.IsVisible()) {
        *aOutNearestScrollClip =
            effectsInfo.mVisibleRect.map([&aMetrics](const nsRect& aRect) {
              return aMetrics.mTransformMatrix.TransformBounds(
                  CSSRect::FromAppUnits(aRect));
            });
      }
    }
  }

  // In the case of an element inside an OOP iframe, |aMetrics.mTransformMatrix|
  // includes the translation information about the root layout scroll offset,
  // thus we use nsIFrame::GetBoundingClientRect rather than
  // nsLayoutUtils::GetBoundingContent.
  return aMetrics.mTransformMatrix.TransformBounds(
      CSSRect::FromAppUnits(frame->GetBoundingClientRect()));
}

static bool ShouldZoomToElement(
    const nsCOMPtr<dom::Element>& aElement,
    const RefPtr<dom::Document>& aInProcessRootContentDocument,
    nsIScrollableFrame* aRootScrollFrame,
    const DoubleTapToZoomMetrics& aMetrics) {
  if (nsIFrame* frame = aElement->GetPrimaryFrame()) {
    if (frame->StyleDisplay()->IsInlineFlow() &&
        // Replaced elements are suitable zoom targets because they act like
        // inline-blocks instead of inline. (textarea's are the specific reason
        // we do this)
        !frame->IsReplaced()) {
      return false;
    }
  }
  // Trying to zoom to the html element will just end up scrolling to the start
  // of the document, return false and we'll run out of elements and just
  // zoomout (without scrolling to the start).
  if (aElement->OwnerDoc() == aInProcessRootContentDocument &&
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
    CSSRect rect = GetBoundingContentRect(
        tableCell, aInProcessRootContentDocument, aRootScrollFrame, aMetrics);
    if (rect.width < 0.3 * aMetrics.mRootScrollableRect.width) {
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
                          const CSSRect& aRootScrollableRect) {
  CSSRect rect =
      CSSRect(std::max(aRootScrollableRect.X(), aRect.X() - aMargin), aRect.Y(),
              aRect.Width() + 2 * aMargin, aRect.Height());
  // Constrict the rect to the screen's right edge
  rect.SetWidth(std::min(rect.Width(), aRootScrollableRect.XMost() - rect.X()));
  return rect;
}

static CSSRect AddVMargin(const CSSRect& aRect, const CSSCoord& aMargin,
                          const CSSRect& aRootScrollableRect) {
  CSSRect rect =
      CSSRect(aRect.X(), std::max(aRootScrollableRect.Y(), aRect.Y() - aMargin),
              aRect.Width(), aRect.Height() + 2 * aMargin);
  // Constrict the rect to the screen's bottom edge
  rect.SetHeight(
      std::min(rect.Height(), aRootScrollableRect.YMost() - rect.Y()));
  return rect;
}

static bool IsReplacedElement(const nsCOMPtr<dom::Element>& aElement) {
  if (nsIFrame* frame = aElement->GetPrimaryFrame()) {
    if (frame->IsReplaced()) {
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
    const RefPtr<dom::Document>& aInProcessRootContentDocument,
    const CSSPoint& aPoint, const DoubleTapToZoomMetrics& aMetrics) {
  // Ensure the layout information we get is up-to-date.
  aInProcessRootContentDocument->FlushPendingNotifications(FlushType::Layout);

  // An empty rect as return value is interpreted as "zoom out".
  const CSSRect zoomOut;

  RefPtr<PresShell> presShell = aInProcessRootContentDocument->GetPresShell();
  if (!presShell) {
    return ZoomTarget{zoomOut, CantZoomOutBehavior::ZoomIn};
  }

  nsIScrollableFrame* rootScrollFrame =
      presShell->GetRootScrollFrameAsScrollable();
  if (!rootScrollFrame) {
    return ZoomTarget{zoomOut, CantZoomOutBehavior::ZoomIn};
  }

  CSSPoint documentRelativePoint =
      aInProcessRootContentDocument->IsTopLevelContentDocument()
          ? CSSPoint::FromAppUnits(ViewportUtils::VisualToLayout(
                CSSPoint::ToAppUnits(aPoint), presShell)) +
                CSSPoint::FromAppUnits(rootScrollFrame->GetScrollPosition())
          : aMetrics.mTransformMatrix.TransformPoint(aPoint);

  nsCOMPtr<dom::Element> element = ElementFromPoint(presShell, aPoint);
  if (!element) {
    return ZoomTarget{zoomOut, CantZoomOutBehavior::ZoomIn, Nothing(),
                      Some(documentRelativePoint)};
  }

  CantZoomOutBehavior cantZoomOutBehavior =
      HasNonPassiveWheelListenerOnAncestor(element)
          ? CantZoomOutBehavior::Nothing
          : CantZoomOutBehavior::ZoomIn;

  while (element && !ShouldZoomToElement(element, aInProcessRootContentDocument,
                                         rootScrollFrame, aMetrics)) {
    element = element->GetFlattenedTreeParentElement();
  }

  if (!element) {
    return ZoomTarget{zoomOut, cantZoomOutBehavior, Nothing(),
                      Some(documentRelativePoint)};
  }

  Maybe<CSSRect> nearestScrollClip;
  CSSRect rect =
      GetBoundingContentRect(element, aInProcessRootContentDocument,
                             rootScrollFrame, aMetrics, &nearestScrollClip);

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

          // In the case of OOP iframes, above |overflowRectCSS| in the iframe
          // documents coords, we need to convert it into the top level coords.
          if (!aInProcessRootContentDocument->IsTopLevelContentDocument()) {
            overflowRectCSS.MoveBy(
                CSSPoint::FromAppUnits(-rootScrollFrame->GetScrollPosition()));
            overflowRectCSS =
                aMetrics.mTransformMatrix.TransformBounds(overflowRectCSS);
          }
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
  if (!rect.IsEmpty() && aMetrics.mVisualViewport.Width() > 0.0f &&
      aMetrics.mVisualViewport.Height() > 0.0f) {
    // Calculate the height of the rect if it had the same aspect ratio as
    // aMetrics.mVisualViewport.
    const float widthRatio = rect.Width() / aMetrics.mVisualViewport.Width();
    float targetHeight = aMetrics.mVisualViewport.Height() * widthRatio;

    // We don't want to cut off the top or bottoms of replaced elements that are
    // square or wider in aspect ratio.

    // If it's a replaced element and we would otherwise trim it's height below
    if (IsReplacedElement(element) && targetHeight < rect.Height() &&
        // If the target rect is at most 1.1x away from being square or wider
        // aspect ratio
        rect.Height() < 1.1 * rect.Width() &&
        // and our aMetrics.mVisualViewport is wider than it is tall
        aMetrics.mVisualViewport.Width() >= aMetrics.mVisualViewport.Height()) {
      heightConstrained = true;
      // Expand the width of the rect so that it fills aMetrics.mVisualViewport
      // so that if we are already zoomed to this element then the
      // IsRectZoomedIn call below returns true so that we zoom out. This won't
      // change what we actually zoom to as we are just making the rect the same
      // aspect ratio as aMetrics.mVisualViewport.
      float targetWidth = rect.Height() * aMetrics.mVisualViewport.Width() /
                          aMetrics.mVisualViewport.Height();
      MOZ_ASSERT(targetWidth > rect.Width());
      if (targetWidth > rect.Width()) {
        rect.x -= (targetWidth - rect.Width()) / 2;
        rect.SetWidth(targetWidth);
        // keep elementBoundingRect containing rect
        elementBoundingRect = rect;
      }

    } else if (targetHeight < rect.Height()) {
      // Trim the height so that the target rect has the same aspect ratio as
      // aMetrics.mVisualViewport, centering it around the user tap point.
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
  rect = AddHMargin(rect, margin, aMetrics.mRootScrollableRect);

  if (heightConstrained) {
    rect = AddVMargin(rect, margin, aMetrics.mRootScrollableRect);
  }

  // If the rect is already taking up most of the visible area and is
  // stretching the width of the page, then we want to zoom out instead.
  if (RectHasAlmostSameZoomLevel(rect, aMetrics.mVisualViewport)) {
    return ZoomTarget{zoomOut, cantZoomOutBehavior, Nothing(),
                      Some(documentRelativePoint)};
  }

  elementBoundingRect =
      AddHMargin(elementBoundingRect, margin, aMetrics.mRootScrollableRect);

  // Unlike rect, elementBoundingRect is the full height of the element we are
  // zooming to. If we zoom to it without a margin it can look a weird, so give
  // it a vertical margin.
  elementBoundingRect =
      AddVMargin(elementBoundingRect, margin, aMetrics.mRootScrollableRect);

  rect.Round();
  elementBoundingRect.Round();

  return ZoomTarget{rect, cantZoomOutBehavior, Some(elementBoundingRect),
                    Some(documentRelativePoint)};
}

std::ostream& operator<<(std::ostream& aStream,
                         const DoubleTapToZoomMetrics& aMetrics) {
  aStream << "{ vv=" << aMetrics.mVisualViewport
          << ", rscr=" << aMetrics.mRootScrollableRect
          << ", transform=" << aMetrics.mTransformMatrix << " }";
  return aStream;
}

}  // namespace layers
}  // namespace mozilla
