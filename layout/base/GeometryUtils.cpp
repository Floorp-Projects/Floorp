/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeometryUtils.h"

#include "mozilla/dom/GeometryUtilsBinding.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Text.h"
#include "mozilla/dom/DOMQuad.h"
#include "nsIFrame.h"
#include "nsGenericDOMDataNode.h"
#include "nsCSSFrameConstructor.h"
#include "nsLayoutUtils.h"
#include "nsSVGUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {

typedef OwningTextOrElementOrDocument GeometryNode;

enum GeometryNodeType {
  GEOMETRY_NODE_ELEMENT,
  GEOMETRY_NODE_TEXT,
  GEOMETRY_NODE_DOCUMENT
};

static nsIFrame*
GetFrameForNode(nsINode* aNode, GeometryNodeType aType)
{
  nsIDocument* doc = aNode->OwnerDoc();
  doc->FlushPendingNotifications(Flush_Layout);
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
GetFrameForGeometryNode(const Optional<GeometryNode>& aGeometryNode,
                        nsINode* aDefaultNode)
{
  if (!aGeometryNode.WasPassed()) {
    return GetFrameForNode(aDefaultNode->OwnerDoc(), GEOMETRY_NODE_DOCUMENT);
  }

  const GeometryNode& value = aGeometryNode.Value();
  if (value.IsElement()) {
    return GetFrameForNode(value.GetAsElement(), GEOMETRY_NODE_ELEMENT);
  }
  if (value.IsDocument()) {
    return GetFrameForNode(value.GetAsDocument(), GEOMETRY_NODE_DOCUMENT);
  }
  return GetFrameForNode(value.GetAsText(), GEOMETRY_NODE_TEXT);
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
GetFirstNonAnonymousFrameForGeometryNode(const Optional<GeometryNode>& aNode,
                                         nsINode* aDefaultNode)
{
  nsIFrame* f = GetFrameForGeometryNode(aNode, aDefaultNode);
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
  if (f) {
    // For SVG, the BoxType is ignored.
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
                         nsTArray<nsRefPtr<DOMQuad> >& aResult,
                         nsIFrame* aRelativeToFrame,
                         const nsPoint& aRelativeToBoxTopLeft,
                         CSSBoxType aBoxType)
    : mParentObject(aParentObject)
    , mResult(aResult)
    , mRelativeToFrame(aRelativeToFrame)
    , mRelativeToBoxTopLeft(aRelativeToBoxTopLeft)
    , mBoxType(aBoxType)
  {
  }

  virtual void AddBox(nsIFrame* aFrame) MOZ_OVERRIDE
  {
    nsIFrame* f = aFrame;
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
  nsTArray<nsRefPtr<DOMQuad> >& mResult;
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
CheckFramesInSameTopLevelBrowsingContext(nsIFrame* aFrame1, nsIFrame* aFrame2)
{
  nsPresContext* pc1 = aFrame1->PresContext();
  nsPresContext* pc2 = aFrame2->PresContext();
  if (pc1 == pc2) {
    return true;
  }
  if (nsContentUtils::IsCallerChrome()) {
    return true;
  }
  if (FindTopLevelPresContext(pc1) == FindTopLevelPresContext(pc2)) {
    return true;
  }
  return false;
}

void GetBoxQuads(nsINode* aNode,
                 const dom::BoxQuadOptions& aOptions,
                 nsTArray<nsRefPtr<DOMQuad> >& aResult,
                 ErrorResult& aRv)
{
  nsIFrame* frame = GetFrameForNode(aNode);
  if (!frame) {
    // No boxes to return
    return;
  }
  nsIDocument* ownerDoc = aNode->OwnerDoc();
  nsIFrame* relativeToFrame =
    GetFirstNonAnonymousFrameForGeometryNode(aOptions.mRelativeTo, ownerDoc);
  if (!relativeToFrame) {
    aRv.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return;
  }
  if (!CheckFramesInSameTopLevelBrowsingContext(frame, relativeToFrame)) {
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

}
