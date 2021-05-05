/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeometryUtils.h"

#include "mozilla/PresShell.h"
#include "mozilla/SVGUtils.h"
#include "mozilla/dom/CharacterData.h"
#include "mozilla/dom/DOMPointBinding.h"
#include "mozilla/dom/GeometryUtilsBinding.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Text.h"
#include "mozilla/dom/DOMPoint.h"
#include "mozilla/dom/DOMQuad.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/BrowserChild.h"
#include "nsIFrame.h"
#include "nsCSSFrameConstructor.h"
#include "nsLayoutUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {

enum GeometryNodeType {
  GEOMETRY_NODE_ELEMENT,
  GEOMETRY_NODE_TEXT,
  GEOMETRY_NODE_DOCUMENT
};

static nsIFrame* GetFrameForNode(nsINode* aNode, GeometryNodeType aType,
                                 bool aCreateFramesForSuppressedWhitespace) {
  Document* doc = aNode->OwnerDoc();
  if (aType == GEOMETRY_NODE_TEXT && aCreateFramesForSuppressedWhitespace) {
    if (PresShell* presShell = doc->GetPresShell()) {
      presShell->FrameConstructor()->EnsureFrameForTextNodeIsCreatedAfterFlush(
          static_cast<CharacterData*>(aNode));
    }
  }
  doc->FlushPendingNotifications(FlushType::Layout);

  switch (aType) {
    case GEOMETRY_NODE_TEXT:
    case GEOMETRY_NODE_ELEMENT:
      return aNode->AsContent()->GetPrimaryFrame();
    case GEOMETRY_NODE_DOCUMENT: {
      PresShell* presShell = doc->GetPresShell();
      return presShell ? presShell->GetRootFrame() : nullptr;
    }
    default:
      MOZ_ASSERT(false, "Unknown GeometryNodeType");
      return nullptr;
  }
}

static nsIFrame* GetFrameForGeometryNode(
    const Optional<OwningGeometryNode>& aGeometryNode, nsINode* aDefaultNode,
    bool aCreateFramesForSuppressedWhitespace) {
  if (!aGeometryNode.WasPassed()) {
    return GetFrameForNode(aDefaultNode->OwnerDoc(), GEOMETRY_NODE_DOCUMENT,
                           aCreateFramesForSuppressedWhitespace);
  }

  const OwningGeometryNode& value = aGeometryNode.Value();
  if (value.IsElement()) {
    return GetFrameForNode(value.GetAsElement(), GEOMETRY_NODE_ELEMENT,
                           aCreateFramesForSuppressedWhitespace);
  }
  if (value.IsDocument()) {
    return GetFrameForNode(value.GetAsDocument(), GEOMETRY_NODE_DOCUMENT,
                           aCreateFramesForSuppressedWhitespace);
  }
  return GetFrameForNode(value.GetAsText(), GEOMETRY_NODE_TEXT,
                         aCreateFramesForSuppressedWhitespace);
}

static nsIFrame* GetFrameForGeometryNode(const GeometryNode& aGeometryNode) {
  // This will create frames for suppressed whitespace nodes.
  if (aGeometryNode.IsElement()) {
    return GetFrameForNode(&aGeometryNode.GetAsElement(), GEOMETRY_NODE_ELEMENT,
                           true);
  }
  if (aGeometryNode.IsDocument()) {
    return GetFrameForNode(&aGeometryNode.GetAsDocument(),
                           GEOMETRY_NODE_DOCUMENT, true);
  }
  return GetFrameForNode(&aGeometryNode.GetAsText(), GEOMETRY_NODE_TEXT, true);
}

static nsIFrame* GetFrameForNode(nsINode* aNode,
                                 bool aCreateFramesForSuppressedWhitespace) {
  if (aNode->IsElement()) {
    return GetFrameForNode(aNode, GEOMETRY_NODE_ELEMENT,
                           aCreateFramesForSuppressedWhitespace);
  }
  if (aNode == aNode->OwnerDoc()) {
    return GetFrameForNode(aNode, GEOMETRY_NODE_DOCUMENT,
                           aCreateFramesForSuppressedWhitespace);
  }
  NS_ASSERTION(aNode->IsText(), "Unknown node type");
  return GetFrameForNode(aNode, GEOMETRY_NODE_TEXT,
                         aCreateFramesForSuppressedWhitespace);
}

static nsIFrame* GetFirstNonAnonymousFrameForGeometryNode(
    const Optional<OwningGeometryNode>& aNode, nsINode* aDefaultNode,
    bool aCreateFramesForSuppressedWhitespace) {
  nsIFrame* f = GetFrameForGeometryNode(aNode, aDefaultNode,
                                        aCreateFramesForSuppressedWhitespace);
  if (f) {
    f = nsLayoutUtils::GetFirstNonAnonymousFrame(f);
  }
  return f;
}

static nsIFrame* GetFirstNonAnonymousFrameForGeometryNode(
    const GeometryNode& aNode) {
  // This will create frames for suppressed whitespace nodes.
  nsIFrame* f = GetFrameForGeometryNode(aNode);
  if (f) {
    f = nsLayoutUtils::GetFirstNonAnonymousFrame(f);
  }
  return f;
}

static nsIFrame* GetFirstNonAnonymousFrameForNode(nsINode* aNode) {
  // This will create frames for suppressed whitespace nodes.
  nsIFrame* f = GetFrameForNode(aNode, true);
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
static nsRect GetBoxRectForFrame(nsIFrame** aFrame, CSSBoxType aType) {
  nsRect r;
  nsIFrame* f = SVGUtils::GetOuterSVGFrameAndCoveredRegion(*aFrame, &r);
  if (f && f != *aFrame) {
    // For non-outer SVG frames, the BoxType is ignored.
    *aFrame = f;
    return r;
  }

  f = *aFrame;
  switch (aType) {
    case CSSBoxType::Content:
      r = f->GetContentRectRelativeToSelf();
      break;
    case CSSBoxType::Padding:
      r = f->GetPaddingRectRelativeToSelf();
      break;
    case CSSBoxType::Border:
      r = nsRect(nsPoint(0, 0), f->GetSize());
      break;
    case CSSBoxType::Margin:
      r = f->GetMarginRectRelativeToSelf();
      break;
    default:
      MOZ_ASSERT(false, "unknown box type");
      return r;
  }

  return r;
}

class AccumulateQuadCallback : public nsLayoutUtils::BoxCallback {
 public:
  AccumulateQuadCallback(Document* aParentObject,
                         nsTArray<RefPtr<DOMQuad> >& aResult,
                         nsIFrame* aRelativeToFrame,
                         const nsPoint& aRelativeToBoxTopLeft,
                         CSSBoxType aBoxType)
      : mParentObject(ToSupports(aParentObject)),
        mResult(aResult),
        mRelativeToFrame(aRelativeToFrame),
        mRelativeToBoxTopLeft(aRelativeToBoxTopLeft),
        mBoxType(aBoxType) {
    if (mBoxType == CSSBoxType::Margin) {
      // Don't include the caption margin when computing margins for a
      // table
      mIncludeCaptionBoxForTable = false;
    }
  }

  virtual void AddBox(nsIFrame* aFrame) override {
    nsIFrame* f = aFrame;
    if (mBoxType == CSSBoxType::Margin && f->IsTableFrame()) {
      // Margin boxes for table frames should be taken from the table wrapper
      // frame, since that has the margin.
      f = f->GetParent();
    }
    nsRect box = GetBoxRectForFrame(&f, mBoxType);
    nsPoint appUnits[4] = {box.TopLeft(), box.TopRight(), box.BottomRight(),
                           box.BottomLeft()};
    CSSPoint points[4];
    for (uint32_t i = 0; i < 4; ++i) {
      points[i] =
          CSSPoint(nsPresContext::AppUnitsToFloatCSSPixels(appUnits[i].x),
                   nsPresContext::AppUnitsToFloatCSSPixels(appUnits[i].y));
    }
    nsLayoutUtils::TransformResult rv =
        nsLayoutUtils::TransformPoints(f, mRelativeToFrame, 4, points);
    if (rv == nsLayoutUtils::TRANSFORM_SUCCEEDED) {
      CSSPoint delta(
          nsPresContext::AppUnitsToFloatCSSPixels(mRelativeToBoxTopLeft.x),
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

static nsPresContext* FindTopLevelPresContext(nsPresContext* aPC) {
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

static bool CheckFramesInSameTopLevelBrowsingContext(nsIFrame* aFrame1,
                                                     nsIFrame* aFrame2,
                                                     CallerType aCallerType) {
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

void GetBoxQuads(nsINode* aNode, const dom::BoxQuadOptions& aOptions,
                 nsTArray<RefPtr<DOMQuad> >& aResult, CallerType aCallerType,
                 ErrorResult& aRv) {
  nsIFrame* frame =
      GetFrameForNode(aNode, aOptions.mCreateFramesForSuppressedWhitespace);
  if (!frame) {
    // No boxes to return
    return;
  }
  AutoWeakFrame weakFrame(frame);
  Document* ownerDoc = aNode->OwnerDoc();
  nsIFrame* relativeToFrame = GetFirstNonAnonymousFrameForGeometryNode(
      aOptions.mRelativeTo, ownerDoc,
      aOptions.mCreateFramesForSuppressedWhitespace);
  // The first frame might be destroyed now if the above call lead to an
  // EnsureFrameForTextNode call.  We need to get the first frame again
  // when that happens and re-check it.
  if (!weakFrame.IsAlive()) {
    frame =
        GetFrameForNode(aNode, aOptions.mCreateFramesForSuppressedWhitespace);
    if (!frame) {
      // No boxes to return
      return;
    }
  }
  if (!relativeToFrame) {
    // XXXbz There's no spec for this.
    return aRv.ThrowNotFoundError("No box to get quads relative to");
  }
  if (!CheckFramesInSameTopLevelBrowsingContext(frame, relativeToFrame,
                                                aCallerType)) {
    aRv.ThrowNotFoundError(
        "Can't get quads relative to a box in a different toplevel browsing "
        "context");
    return;
  }
  // GetBoxRectForFrame can modify relativeToFrame so call it first.
  nsPoint relativeToTopLeft =
      GetBoxRectForFrame(&relativeToFrame, CSSBoxType::Border).TopLeft();
  AccumulateQuadCallback callback(ownerDoc, aResult, relativeToFrame,
                                  relativeToTopLeft, aOptions.mBox);

  // Bug 1624653: Refactor this to get boxes in layer pixels, which we will
  // then convert into CSS units.
  nsLayoutUtils::GetAllInFlowBoxes(frame, &callback);
}

void GetBoxQuadsFromWindowOrigin(nsINode* aNode,
                                 const dom::BoxQuadOptions& aOptions,
                                 nsTArray<RefPtr<DOMQuad> >& aResult,
                                 ErrorResult& aRv) {
  // We want the quads relative to the window. To do this, we ignore the
  // provided aOptions.mRelativeTo and instead use the document node of
  // the top-most in-process document. Later, we'll check if there is a
  // browserChild associated with that document, and if so, transform the
  // calculated quads with the browserChild's to-parent matrix, which
  // will get us to top-level coordinates.
  if (aOptions.mRelativeTo.WasPassed()) {
    return aRv.ThrowNotSupportedError(
        "Can't request quads in window origin space relative to another "
        "node.");
  }

  // We're going to call GetBoxQuads with our parameters, but we supply
  // a new BoxQuadOptions object that uses the top in-process document
  // as the relativeTo target.
  BoxQuadOptions bqo(aOptions);

  RefPtr<Document> topInProcessDoc =
      nsContentUtils::GetInProcessSubtreeRootDocument(aNode->OwnerDoc());

  OwningGeometryNode ogn;
  ogn.SetAsDocument() = topInProcessDoc;
  bqo.mRelativeTo.Construct(ogn);

  // Bug 1624653: Refactor this to get boxes in layer pixels, which we can
  // transform directly with the GetChildToParentConversionMatrix below,
  // and convert to CSS units as a final step.
  GetBoxQuads(aNode, bqo, aResult, CallerType::System, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Now we have aResult filled with DOMQuads with values relative to the
  // top in-process document. See if topInProcessDoc is associated with a
  // BrowserChild, and if it is, get its transformation matrix and use that
  // to transform the DOMQuads in place to make them relative to the window
  // origin.
  nsIDocShell* docShell = topInProcessDoc->GetDocShell();
  if (!docShell) {
    return aRv.ThrowInvalidStateError(
        "Returning untranslated quads because top in process document has "
        "no docshell.");
  }

  BrowserChild* browserChild = BrowserChild::GetFrom(docShell);
  if (!browserChild) {
    return;
  }

  nsPresContext* presContext = docShell->GetPresContext();
  if (!presContext) {
    return;
  }
  int32_t appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();

  LayoutDeviceToLayoutDeviceMatrix4x4 matrix =
      browserChild->GetChildToParentConversionMatrix();

  // For each DOMQuad in aResult, change the css units into layer pixels,
  // then transform them by matrix, then change them back into css units
  // and overwrite the original points.
  LayoutDeviceToCSSScale ld2cScale((float)appUnitsPerDevPixel /
                                   (float)AppUnitsPerCSSPixel());
  CSSToLayoutDeviceScale c2ldScale = ld2cScale.Inverse();

  for (auto& quad : aResult) {
    for (uint32_t i = 0; i < 4; i++) {
      DOMPoint* p = quad->Point(i);
      CSSPoint cp(p->X(), p->Y());

      LayoutDevicePoint windowLdp = matrix.TransformPoint(cp * c2ldScale);

      CSSPoint windowCp = windowLdp * ld2cScale;
      p->SetX(windowCp.x);
      p->SetY(windowCp.y);
    }
  }
}

static void TransformPoints(nsINode* aTo, const GeometryNode& aFrom,
                            uint32_t aPointCount, CSSPoint* aPoints,
                            const ConvertCoordinateOptions& aOptions,
                            CallerType aCallerType, ErrorResult& aRv) {
  nsIFrame* fromFrame = GetFirstNonAnonymousFrameForGeometryNode(aFrom);
  AutoWeakFrame weakFrame(fromFrame);
  nsIFrame* toFrame = GetFirstNonAnonymousFrameForNode(aTo);
  // The first frame might be destroyed now if the above call lead to an
  // EnsureFrameForTextNode call.  We need to get the first frame again
  // when that happens.
  if (fromFrame && !weakFrame.IsAlive()) {
    fromFrame = GetFirstNonAnonymousFrameForGeometryNode(aFrom);
  }
  if (!fromFrame || !toFrame) {
    aRv.ThrowNotFoundError(
        "Can't transform coordinates between nonexistent boxes");
    return;
  }
  if (!CheckFramesInSameTopLevelBrowsingContext(fromFrame, toFrame,
                                                aCallerType)) {
    aRv.ThrowNotFoundError(
        "Can't transform coordinates between boxes in different toplevel "
        "browsing contexts");
    return;
  }

  nsPoint fromOffset =
      GetBoxRectForFrame(&fromFrame, aOptions.mFromBox).TopLeft();
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

already_AddRefed<DOMQuad> ConvertQuadFromNode(
    nsINode* aTo, dom::DOMQuad& aQuad, const GeometryNode& aFrom,
    const dom::ConvertCoordinateOptions& aOptions, CallerType aCallerType,
    ErrorResult& aRv) {
  CSSPoint points[4];
  for (uint32_t i = 0; i < 4; ++i) {
    DOMPoint* p = aQuad.Point(i);
    if (p->W() != 1.0 || p->Z() != 0.0) {
      aRv.ThrowInvalidStateError("Point is not 2d");
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

already_AddRefed<DOMQuad> ConvertRectFromNode(
    nsINode* aTo, dom::DOMRectReadOnly& aRect, const GeometryNode& aFrom,
    const dom::ConvertCoordinateOptions& aOptions, CallerType aCallerType,
    ErrorResult& aRv) {
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

already_AddRefed<DOMPoint> ConvertPointFromNode(
    nsINode* aTo, const dom::DOMPointInit& aPoint, const GeometryNode& aFrom,
    const dom::ConvertCoordinateOptions& aOptions, CallerType aCallerType,
    ErrorResult& aRv) {
  if (aPoint.mW != 1.0 || aPoint.mZ != 0.0) {
    aRv.ThrowInvalidStateError("Point is not 2d");
    return nullptr;
  }
  CSSPoint point(aPoint.mX, aPoint.mY);
  TransformPoints(aTo, aFrom, 1, &point, aOptions, aCallerType, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  RefPtr<DOMPoint> result =
      new DOMPoint(aTo->GetParentObject().mObject, point.x, point.y);
  return result.forget();
}

}  // namespace mozilla
