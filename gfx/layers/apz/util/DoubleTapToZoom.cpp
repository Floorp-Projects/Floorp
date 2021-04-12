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
#include "nsLayoutUtils.h"
#include "nsStyleConsts.h"
#include "mozilla/ViewportUtils.h"

namespace mozilla {
namespace layers {

namespace {

using FrameForPointOption = nsLayoutUtils::FrameForPointOption;

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
                   frame->GetContent()->IsInNativeAnonymousSubtree())) {
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

static bool ShouldZoomToElement(
    const nsCOMPtr<dom::Element>& aElement,
    const RefPtr<dom::Document>& aRootContentDocument) {
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
  return true;
}

static bool IsRectZoomedIn(const CSSRect& aRect,
                           const CSSRect& aCompositedArea) {
  // This functions checks to see if the area of the rect visible in the
  // composition bounds (i.e. the overlapArea variable below) is approximately
  // the max area of the rect we can show.
  CSSRect overlap = aCompositedArea.Intersect(aRect);
  float overlapArea = overlap.Width() * overlap.Height();
  float availHeight = std::min(
      aRect.Width() * aCompositedArea.Height() / aCompositedArea.Width(),
      aRect.Height());
  float showing = overlapArea / (aRect.Width() * availHeight);
  float ratioW = aRect.Width() / aCompositedArea.Width();
  float ratioH = aRect.Height() / aCompositedArea.Height();

  return showing > 0.9 && (ratioW > 0.9 || ratioH > 0.9);
}

}  // namespace

CSSRect CalculateRectToZoomTo(const RefPtr<dom::Document>& aRootContentDocument,
                              const CSSPoint& aPoint) {
  // Ensure the layout information we get is up-to-date.
  aRootContentDocument->FlushPendingNotifications(FlushType::Layout);

  // An empty rect as return value is interpreted as "zoom out".
  const CSSRect zoomOut;

  RefPtr<PresShell> presShell = aRootContentDocument->GetPresShell();
  if (!presShell) {
    return zoomOut;
  }

  nsIScrollableFrame* rootScrollFrame =
      presShell->GetRootScrollFrameAsScrollable();
  if (!rootScrollFrame) {
    return zoomOut;
  }

  nsCOMPtr<dom::Element> element = ElementFromPoint(presShell, aPoint);
  if (!element) {
    return zoomOut;
  }

  while (element && !ShouldZoomToElement(element, aRootContentDocument)) {
    element = element->GetParentElement();
  }

  if (!element) {
    return zoomOut;
  }

  FrameMetrics metrics =
      nsLayoutUtils::CalculateBasicFrameMetrics(rootScrollFrame);
  CSSPoint visualScrollOffset = metrics.GetVisualScrollOffset();
  CSSRect compositedArea(visualScrollOffset,
                         metrics.CalculateCompositedSizeInCssPixels());
  const CSSCoord margin = 15;
  Maybe<CSSRect> nearestScrollClip;
  CSSRect rect = nsLayoutUtils::GetBoundingContentRect(element, rootScrollFrame,
                                                       &nearestScrollClip);

  CSSPoint point = CSSPoint::FromAppUnits(
      ViewportUtils::VisualToLayout(CSSPoint::ToAppUnits(aPoint), presShell));

  CSSPoint documentRelativePoint =
      point + CSSPoint::FromAppUnits(rootScrollFrame->GetScrollPosition());

  // In some cases, like overflow: visible and overflowing content, the bounding
  // client rect of the targeted element won't contain the point the user double
  // tapped on. In that case we use the scrollable overflow rect if it contains
  // the user point.
  if (!rect.Contains(documentRelativePoint)) {
    if (nsIFrame* scrolledFrame = rootScrollFrame->GetScrolledFrame()) {
      if (nsIFrame* f = element->GetPrimaryFrame()) {
        CSSRect overflowRect =
            CSSRect::FromAppUnits(nsLayoutUtils::TransformFrameRectToAncestor(
                f, f->ScrollableOverflowRect(),
                RelativeTo{scrolledFrame, ViewportType::Layout}));
        if (nearestScrollClip.isSome()) {
          overflowRect = nearestScrollClip->Intersect(overflowRect);
        }
        if (overflowRect.Contains(documentRelativePoint)) {
          rect = overflowRect;
        }
      }
    }
  }

  // If the element is taller than the visible area of the page scale
  // the height of the |rect| so that it has the same aspect ratio as
  // the root frame.  The clipped |rect| is centered on the y value of
  // the touch point. This allows tall narrow elements to be zoomed.
  if (!rect.IsEmpty() && compositedArea.Width() > 0.0f) {
    const float widthRatio = rect.Width() / compositedArea.Width();
    float targetHeight = compositedArea.Height() * widthRatio;
    if (widthRatio < 0.9 && targetHeight < rect.Height()) {
      float newY = documentRelativePoint.y - (targetHeight * 0.5f);
      if ((newY + targetHeight) > rect.YMost()) {
        rect.MoveByY(rect.Height() - targetHeight);
      } else if (newY > rect.Y()) {
        rect.MoveToY(newY);
      }
      rect.SetHeight(targetHeight);
    }
  }

  rect = CSSRect(std::max(metrics.GetScrollableRect().X(), rect.X() - margin),
                 rect.Y(), rect.Width() + 2 * margin, rect.Height());
  // Constrict the rect to the screen's right edge
  rect.SetWidth(
      std::min(rect.Width(), metrics.GetScrollableRect().XMost() - rect.X()));

  // If the rect is already taking up most of the visible area and is
  // stretching the width of the page, then we want to zoom out instead.
  if (IsRectZoomedIn(rect, compositedArea)) {
    return zoomOut;
  }

  rect.Round();
  return rect;
}

}  // namespace layers
}  // namespace mozilla
