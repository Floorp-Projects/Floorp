/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeometryUtils.h"

#include "mozilla/dom/DOMPointBinding.h"
#include "mozilla/dom/GeometryUtilsBinding.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Text.h"
#include "mozilla/dom/DOMPoint.h"
#include "mozilla/dom/DOMQuad.h"
#include "mozilla/dom/DOMRect.h"
#include "nsContentUtils.h"
#include "nsIFrame.h"
#include "nsGenericDOMDataNode.h"
#include "nsCSSFrameConstructor.h"
#include "nsLayoutUtils.h"
#include "nsSVGUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {

enum GeometryNodeType {
  GEOMETRY_NODE_ELEMENT,
  GEOMETRY_NODE_TEXT,
  GEOMETRY_NODE_DOCUMENT
};

static nsIFrame*
GetFrameForNode(nsINode* aNode, GeometryNodeType aType)
{
  nsIDocument* doc = aNode->OwnerDoc();
  doc->FlushPendingNotifications(FlushType::Layout);
  switch (aType) {
  case GEOMETRY_NODE_ELEMENT:
    return aNode->AsContent()->GetPrimaryFrame();
  case GEOMETRY_NODE_TEXT: {
    nsIPresShell* presShell = doc->GetShell();
    if (presShell) {
      return presShell->FrameConstructor()->EnsureFrameForTextNode(
          static_cast<nsGenericDOMDataNode*>(aNode));
    }
    return nullptr;
  }
  case GEOMETRY_NODE_DOCUMENT: {
    nsIPresShell* presShell = doc->GetShell();
    return presShell ? presShell->GetRootFrame() : nullptr;
  }
  default:
    MOZ_ASSERT(false, "Unknown GeometryNodeType");
    return nullptr;
  }
}

static nsIFrame*
GetFrameForGeometryNode(const Optional<OwningGeometryNode>& aGeometryNode,
                        nsINode* aDefaultNode)
{
  if (!aGeometryNode.WasPassed()) {
    return GetFrameForNode(aDefaultNode->OwnerDoc(), GEOMETRY_NODE_DOCUMENT);
  }

  const OwningGeometryNode& value = aGeometryNode.Value();
  if (value.IsElement()) {
    return GetFrameForNode(value.GetAsElement(), GEOMETRY_NODE_ELEMENT);
  }
  if (value.IsDocument()) {
    return GetFrameForNode(value.GetAsDocument(), GEOMETRY_NODE_DOCUMENT);
  }
  return GetFrameForNode(value.GetAsText(), GEOMETRY_NODE_TEXT);
}

static nsIFrame*
GetFrameForGeometryNode(const GeometryNode& aGeometryNode)
{
  if (aGeometryNode.IsElement()) {
    return GetFrameForNode(&aGeometryNode.GetAsElement(), GEOMETRY_NODE_ELEMENT);
  }
  if (aGeometryNode.IsDocument()) {
    return GetFrameForNode(&aGeometryNode.GetAsDocument(), GEOMETRY_NODE_DOCUMENT);
  }
  return GetFrameForNode(&aGeometryNode.GetAsText(), GEOMETRY_NODE_TEXT);
}

static nsIFrame*
GetFrameForNode(nsINode* aNode)
{
  if (aNode->IsElement()) {
    return GetFrameForNode(aNode, GEOMETRY_NODE_ELEMENT);
  }
  if (aNode == aNode->OwnerDoc()) {
    return GetFrameForNode(aNode, GEOMETRY_NODE_DOCUMENT);
  }
  NS_ASSERTION(aNode->IsNodeOfType(nsINode::eTEXT), "Unknown node type");
  return GetFrameForNode(aNode, GEOMETRY_NODE_TEXT);
}

static nsIFrame*
GetFirstNonAnonymousFrameForGeometryNode(const Optional<OwningGeometryNode>& aNode,
                                         nsINode* aDefaultNode)
{
  nsIFrame* f = GetFrameForGeometryNode(aNode, aDefaultNode);
  if (f) {
    f = nsLayoutUtils::GetFirstNonAnonymousFrame(f);
  }
  return f;
}

static nsIFrame*
GetFirstNonAnonymousFrameForGeometryNode(const GeometryNode& aNode)
{
  nsIFrame* f = GetFrameForGeometryNode(aNode);
  if (f) {
    f = nsLayoutUtils::GetFirstNonAnonymousFrame(f);
  }
  return f;
}

static nsIFrame*
GetFirstNonAnonymousFrameForNode(nsINode* aNode)
{
  nsIFrame* f = GetFrameForNode(aNode);
  if (f) {
    f = nsLayoutUtils::GetFirstNonAnonymousFrame(f);
  }
  return f;
}

/**
 * This can modify aFrame to point to a different frame. This is needed to
 * handle SVG, where SVG elements can only compute a rect that's valid with
 * respect to the "outer SVG" frame.
 */
static nsRect
GetBoxRectForFrame(nsIFrame** aFrame, CSSBoxType aType)
{
  nsRect r;
  nsIFrame* f = nsSVGUtils::GetOuterSVGFrameAndCoveredRegion(*aFrame, &r);
  if (f && f != *aFrame) {
    // For non-outer SVG frames, the BoxType is ignored.
    *aFrame = f;
    return r;
  }

  f = *aFrame;
  switch (aType) {
  case CSSBoxType::Content: r = f->GetContentRectRelativeToSelf(); break;
  case CSSBoxType::Padding: r = f->GetPaddingRectRelativeToSelf(); break;
  case CSSBoxType::Border: r = nsRect(nsPoint(0, 0), f->GetSize()); break;
  case CSSBoxType::Margin: {
    r = nsRect(nsPoint(0, 0), f->GetSize());
    r.Inflate(f->GetUsedMargin());
    break;
  }
  default: MOZ_ASSERT(false, "unknown box type"); return r;
  }

  return r;
}

class AccumulateQuadCallback : public nsLayoutUtils::BoxCallback {
public:
  AccumulateQuadCallback(nsISupports* aParentObject,
                         nsTArray<RefPtr<DOMQuad> >& aResult,
                         nsIFrame* aRelativeToFrame,
                         const nsPoint& aRelativeToBoxTopLeft,
                         CSSBoxType aBoxType)
    : mParentObject(aParentObject)
    , mResult(aResult)
    , mRelativeToFrame(aRelativeToFrame)
    , mRelativeToBoxTopLeft(aRelativeToBoxTopLeft)
    , mBoxType(aBoxType)
  {
    if (mBoxType == CSSBoxType::Margin) {
      // Don't include the caption margin when computing margins for a
      // table
      mIncludeCaptionBoxForTable = false;
    }
  }

  virtual void AddBox(nsIFrame* aFrame) override
  {
    nsIFrame* f = aFrame;
    if (mBoxType == CSSBoxType::Margin &&
        f->GetType() == nsGkAtoms::tableFrame) {
      // Margin boxes for table frames should be taken from the table wrapper
      // frame, since that has the margin.
      f = f->GetParent();
    }
    nsRect box = GetBoxRectForFrame(&f, mBoxType);
    nsPoint appUnits[4] =
      { box.TopLeft(), box.TopRight(), box.BottomRight(), box.BottomLeft() };
    CSSPoint points[4];
    for (uint32_t i = 0; i < 4; ++i) {
      points[i] = CSSPoint(nsPresContext::AppUnitsToFloatCSSPixels(appUnits[i].x),
                           nsPresContext::AppUnitsToFloatCSSPixels(appUnits[i].y));
    }
    nsLayoutUtils::TransformResult rv =
      nsLayoutUtils::TransformPoints(f, mRelativeToFrame, 4, points);
    if (rv == nsLayoutUtils::TRANSFORM_SUCCEEDED) {
      CSSPoint delta(nsPresContext::AppUnitsToFloatCSSPixels(mRelativeToBoxTopLeft.x),
                     nsPresContext::AppUnitsToFloatCSSPixels(mRelativeToBoxTopLeft.y));
      for (uint32_t i = 0; i < 4; ++i) {
        points[i] -= delta;
      }
    } else {
      PodArrayZero(points);
    }
    mResult.AppendElement(new DOMQuad(mParentObject, points));
  }

  nsISupports* mParentObject;
  nsTArray<RefPtr<DOMQuad> >& mResult;
  nsIFrame* mRelativeToFrame;
  nsPoint mRelativeToBoxTopLeft;
  CSSBoxType mBoxType;
};

static nsPresContext*
FindTopLevelPresContext(nsPresContext* aPC)
{
  bool isChrome = aPC->IsChrome();
  nsPresContext* pc = aPC;
  for (;;) {
    nsPresContext* parent = pc->GetParentPresContext();
    if (!parent || parent->IsChrome() != isChrome) {
      return pc;
    }
    pc = parent;
  }
}

static bool
CheckFramesInSameTopLevelBrowsingContext(nsIFrame* aFrame1, nsIFrame* aFrame2,
                                         CallerType aCallerType)
{
  nsPresContext* pc1 = aFrame1->PresContext();
  nsPresContext* pc2 = aFrame2->PresContext();
  if (pc1 == pc2) {
    return true;
  }
  if (aCallerType == CallerType::System) {
    return true;
  }
  if (FindTopLevelPresContext(pc1) == FindTopLevelPresContext(pc2)) {
    return true;
  }
  return false;
}

void GetBoxQuads(nsINode* aNode,
                 const dom::BoxQuadOptions& aOptions,
                 nsTArray<RefPtr<DOMQuad> >& aResult,
                 CallerType aCallerType,
                 ErrorResult& aRv)
{
  nsIFrame* frame = GetFrameForNode(aNode);
  if (!frame) {
    // No boxes to return
    return;
  }
  nsWeakFrame weakFrame(frame);
  nsIDocument* ownerDoc = aNode->OwnerDoc();
  nsIFrame* relativeToFrame =
    GetFirstNonAnonymousFrameForGeometryNode(aOptions.mRelativeTo, ownerDoc);
  // The first frame might be destroyed now if the above call lead to an
  // EnsureFrameForTextNode call.  We need to get the first frame again
  // when that happens and re-check it.
  if (!weakFrame.IsAlive()) {
    frame = GetFrameForNode(aNode);
    if (!frame) {
      // No boxes to return
      return;
    }
  }
  if (!relativeToFrame) {
    aRv.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return;
  }
  if (!CheckFramesInSameTopLevelBrowsingContext(frame, relativeToFrame,
                                                aCallerType)) {
    aRv.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return;
  }
  // GetBoxRectForFrame can modify relativeToFrame so call it first.
  nsPoint relativeToTopLeft =
      GetBoxRectForFrame(&relativeToFrame, CSSBoxType::Border).TopLeft();
  AccumulateQuadCallback callback(ownerDoc, aResult, relativeToFrame,
                                  relativeToTopLeft, aOptions.mBox);
  nsLayoutUtils::GetAllInFlowBoxes(frame, &callback);
}

static void
TransformPoints(nsINode* aTo, const GeometryNode& aFrom,
                uint32_t aPointCount, CSSPoint* aPoints,
                const ConvertCoordinateOptions& aOptions,
                CallerType aCallerType, ErrorResult& aRv)
{
  nsIFrame* fromFrame = GetFirstNonAnonymousFrameForGeometryNode(aFrom);
  nsWeakFrame weakFrame(fromFrame);
  nsIFrame* toFrame = GetFirstNonAnonymousFrameForNode(aTo);
  // The first frame might be destroyed now if the above call lead to an
  // EnsureFrameForTextNode call.  We need to get the first frame again
  // when that happens.
  if (fromFrame && !weakFrame.IsAlive()) {
    fromFrame = GetFirstNonAnonymousFrameForGeometryNode(aFrom);
  }
  if (!fromFrame || !toFrame) {
    aRv.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return;
  }
  if (!CheckFramesInSameTopLevelBrowsingContext(fromFrame, toFrame, aCallerType)) {
    aRv.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return;
  }

  nsPoint fromOffset = GetBoxRectForFrame(&fromFrame, aOptions.mFromBox).TopLeft();
  nsPoint toOffset = GetBoxRectForFrame(&toFrame, aOptions.mToBox).TopLeft();
  CSSPoint fromOffsetGfx(nsPresContext::AppUnitsToFloatCSSPixels(fromOffset.x),
                         nsPresContext::AppUnitsToFloatCSSPixels(fromOffset.y));
  for (uint32_t i = 0; i < aPointCount; ++i) {
    aPoints[i] += fromOffsetGfx;
  }
  nsLayoutUtils::TransformResult rv =
    nsLayoutUtils::TransformPoints(fromFrame, toFrame, aPointCount, aPoints);
  if (rv == nsLayoutUtils::TRANSFORM_SUCCEEDED) {
    CSSPoint toOffsetGfx(nsPresContext::AppUnitsToFloatCSSPixels(toOffset.x),
                         nsPresContext::AppUnitsToFloatCSSPixels(toOffset.y));
    for (uint32_t i = 0; i < aPointCount; ++i) {
      aPoints[i] -= toOffsetGfx;
    }
  } else {
    PodZero(aPoints, aPointCount);
  }
}

already_AddRefed<DOMQuad>
ConvertQuadFromNode(nsINode* aTo, dom::DOMQuad& aQuad,
                    const GeometryNode& aFrom,
                    const dom::ConvertCoordinateOptions& aOptions,
                    CallerType aCallerType,
                    ErrorResult& aRv)
{
  CSSPoint points[4];
  for (uint32_t i = 0; i < 4; ++i) {
    DOMPoint* p = aQuad.Point(i);
    if (p->W() != 1.0 || p->Z() != 0.0) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return nullptr;
    }
    points[i] = CSSPoint(p->X(), p->Y());
  }
  TransformPoints(aTo, aFrom, 4, points, aOptions, aCallerType, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  RefPtr<DOMQuad> result = new DOMQuad(aTo->GetParentObject().mObject, points);
  return result.forget();
}

already_AddRefed<DOMQuad>
ConvertRectFromNode(nsINode* aTo, dom::DOMRectReadOnly& aRect,
                    const GeometryNode& aFrom,
                    const dom::ConvertCoordinateOptions& aOptions,
                    CallerType aCallerType,
                    ErrorResult& aRv)
{
  CSSPoint points[4];
  double x = aRect.X(), y = aRect.Y(), w = aRect.Width(), h = aRect.Height();
  points[0] = CSSPoint(x, y);
  points[1] = CSSPoint(x + w, y);
  points[2] = CSSPoint(x + w, y + h);
  points[3] = CSSPoint(x, y + h);
  TransformPoints(aTo, aFrom, 4, points, aOptions, aCallerType, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  RefPtr<DOMQuad> result = new DOMQuad(aTo->GetParentObject().mObject, points);
  return result.forget();
}

already_AddRefed<DOMPoint>
ConvertPointFromNode(nsINode* aTo, const dom::DOMPointInit& aPoint,
                     const GeometryNode& aFrom,
                     const dom::ConvertCoordinateOptions& aOptions,
                     CallerType aCallerType,
                     ErrorResult& aRv)
{
  if (aPoint.mW != 1.0 || aPoint.mZ != 0.0) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  CSSPoint point(aPoint.mX, aPoint.mY);
  TransformPoints(aTo, aFrom, 1, &point, aOptions, aCallerType, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  RefPtr<DOMPoint> result = new DOMPoint(aTo->GetParentObject().mObject, point.x, point.y);
  return result.forget();
}

} // namespace mozilla
