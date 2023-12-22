/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGPathElement.h"

#include <algorithm>

#include "DOMSVGPathSeg.h"
#include "DOMSVGPathSegList.h"
#include "SVGGeometryProperty.h"
#include "gfx2DGlue.h"
#include "gfxPlatform.h"
#include "nsGkAtoms.h"
#include "nsIFrame.h"
#include "nsStyleConsts.h"
#include "nsStyleStruct.h"
#include "nsWindowSizes.h"
#include "mozilla/dom/SVGPathElementBinding.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/SVGContentUtils.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(Path)

using namespace mozilla::gfx;

namespace mozilla::dom {

JSObject* SVGPathElement::WrapNode(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return SVGPathElement_Binding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------
// Implementation

SVGPathElement::SVGPathElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGPathElementBase(std::move(aNodeInfo)) {}

//----------------------------------------------------------------------
// memory reporting methods

void SVGPathElement::AddSizeOfExcludingThis(nsWindowSizes& aSizes,
                                            size_t* aNodeSize) const {
  SVGPathElementBase::AddSizeOfExcludingThis(aSizes, aNodeSize);
  *aNodeSize += mD.SizeOfExcludingThis(aSizes.mState.mMallocSizeOf);
}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGPathElement)

uint32_t SVGPathElement::GetPathSegAtLength(float distance) {
  uint32_t seg = 0;
  auto callback = [&](const ComputedStyle* s) {
    const nsStyleSVGReset* styleSVGReset = s->StyleSVGReset();
    if (styleSVGReset->mD.IsPath()) {
      seg = SVGPathData::GetPathSegAtLength(
          styleSVGReset->mD.AsPath()._0.AsSpan(), distance);
    }
  };

  FlushStyleIfNeeded();
  if (SVGGeometryProperty::DoForComputedStyle(this, callback)) {
    return seg;
  }
  return mD.GetAnimValue().GetPathSegAtLength(distance);
}

already_AddRefed<DOMSVGPathSegClosePath>
SVGPathElement::CreateSVGPathSegClosePath() {
  return do_AddRef(new DOMSVGPathSegClosePath());
}

already_AddRefed<DOMSVGPathSegMovetoAbs>
SVGPathElement::CreateSVGPathSegMovetoAbs(float x, float y) {
  return do_AddRef(new DOMSVGPathSegMovetoAbs(x, y));
}

already_AddRefed<DOMSVGPathSegMovetoRel>
SVGPathElement::CreateSVGPathSegMovetoRel(float x, float y) {
  return do_AddRef(new DOMSVGPathSegMovetoRel(x, y));
}

already_AddRefed<DOMSVGPathSegLinetoAbs>
SVGPathElement::CreateSVGPathSegLinetoAbs(float x, float y) {
  return do_AddRef(new DOMSVGPathSegLinetoAbs(x, y));
}

already_AddRefed<DOMSVGPathSegLinetoRel>
SVGPathElement::CreateSVGPathSegLinetoRel(float x, float y) {
  return do_AddRef(new DOMSVGPathSegLinetoRel(x, y));
}

already_AddRefed<DOMSVGPathSegCurvetoCubicAbs>
SVGPathElement::CreateSVGPathSegCurvetoCubicAbs(float x, float y, float x1,
                                                float y1, float x2, float y2) {
  // Note that we swap from DOM API argument order to the argument order used
  // in the <path> element's 'd' attribute (i.e. we put the arguments for the
  // end point of the segment last instead of first).
  return do_AddRef(new DOMSVGPathSegCurvetoCubicAbs(x1, y1, x2, y2, x, y));
}

already_AddRefed<DOMSVGPathSegCurvetoCubicRel>
SVGPathElement::CreateSVGPathSegCurvetoCubicRel(float x, float y, float x1,
                                                float y1, float x2, float y2) {
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  return do_AddRef(new DOMSVGPathSegCurvetoCubicRel(x1, y1, x2, y2, x, y));
}

already_AddRefed<DOMSVGPathSegCurvetoQuadraticAbs>
SVGPathElement::CreateSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1,
                                                    float y1) {
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  return do_AddRef(new DOMSVGPathSegCurvetoQuadraticAbs(x1, y1, x, y));
}

already_AddRefed<DOMSVGPathSegCurvetoQuadraticRel>
SVGPathElement::CreateSVGPathSegCurvetoQuadraticRel(float x, float y, float x1,
                                                    float y1) {
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  return do_AddRef(new DOMSVGPathSegCurvetoQuadraticRel(x1, y1, x, y));
}

already_AddRefed<DOMSVGPathSegArcAbs> SVGPathElement::CreateSVGPathSegArcAbs(
    float x, float y, float r1, float r2, float angle, bool largeArcFlag,
    bool sweepFlag) {
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  return do_AddRef(
      new DOMSVGPathSegArcAbs(r1, r2, angle, largeArcFlag, sweepFlag, x, y));
}

already_AddRefed<DOMSVGPathSegArcRel> SVGPathElement::CreateSVGPathSegArcRel(
    float x, float y, float r1, float r2, float angle, bool largeArcFlag,
    bool sweepFlag) {
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  return do_AddRef(
      new DOMSVGPathSegArcRel(r1, r2, angle, largeArcFlag, sweepFlag, x, y));
}

already_AddRefed<DOMSVGPathSegLinetoHorizontalAbs>
SVGPathElement::CreateSVGPathSegLinetoHorizontalAbs(float x) {
  return do_AddRef(new DOMSVGPathSegLinetoHorizontalAbs(x));
}

already_AddRefed<DOMSVGPathSegLinetoHorizontalRel>
SVGPathElement::CreateSVGPathSegLinetoHorizontalRel(float x) {
  return do_AddRef(new DOMSVGPathSegLinetoHorizontalRel(x));
}

already_AddRefed<DOMSVGPathSegLinetoVerticalAbs>
SVGPathElement::CreateSVGPathSegLinetoVerticalAbs(float y) {
  return do_AddRef(new DOMSVGPathSegLinetoVerticalAbs(y));
}

already_AddRefed<DOMSVGPathSegLinetoVerticalRel>
SVGPathElement::CreateSVGPathSegLinetoVerticalRel(float y) {
  return do_AddRef(new DOMSVGPathSegLinetoVerticalRel(y));
}

already_AddRefed<DOMSVGPathSegCurvetoCubicSmoothAbs>
SVGPathElement::CreateSVGPathSegCurvetoCubicSmoothAbs(float x, float y,
                                                      float x2, float y2) {
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  return do_AddRef(new DOMSVGPathSegCurvetoCubicSmoothAbs(x2, y2, x, y));
}

already_AddRefed<DOMSVGPathSegCurvetoCubicSmoothRel>
SVGPathElement::CreateSVGPathSegCurvetoCubicSmoothRel(float x, float y,
                                                      float x2, float y2) {
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  return do_AddRef(new DOMSVGPathSegCurvetoCubicSmoothRel(x2, y2, x, y));
}

already_AddRefed<DOMSVGPathSegCurvetoQuadraticSmoothAbs>
SVGPathElement::CreateSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y) {
  return do_AddRef(new DOMSVGPathSegCurvetoQuadraticSmoothAbs(x, y));
}

already_AddRefed<DOMSVGPathSegCurvetoQuadraticSmoothRel>
SVGPathElement::CreateSVGPathSegCurvetoQuadraticSmoothRel(float x, float y) {
  return do_AddRef(new DOMSVGPathSegCurvetoQuadraticSmoothRel(x, y));
}

// FIXME: This API is enabled only if dom.svg.pathSeg.enabled is true. This
// preference is off by default in Bug 1388931, and will be dropped later.
// So we are not planning to map d property for this API.
already_AddRefed<DOMSVGPathSegList> SVGPathElement::PathSegList() {
  return DOMSVGPathSegList::GetDOMWrapper(mD.GetBaseValKey(), this);
}

// FIXME: This API is enabled only if dom.svg.pathSeg.enabled is true. This
// preference is off by default in Bug 1388931, and will be dropped later.
// So we are not planning to map d property for this API.
already_AddRefed<DOMSVGPathSegList> SVGPathElement::AnimatedPathSegList() {
  return DOMSVGPathSegList::GetDOMWrapper(mD.GetAnimValKey(), this);
}

//----------------------------------------------------------------------
// SVGElement methods

/* virtual */
bool SVGPathElement::HasValidDimensions() const {
  bool hasPath = false;
  auto callback = [&](const ComputedStyle* s) {
    const nsStyleSVGReset* styleSVGReset = s->StyleSVGReset();
    hasPath =
        styleSVGReset->mD.IsPath() && !styleSVGReset->mD.AsPath()._0.IsEmpty();
  };

  SVGGeometryProperty::DoForComputedStyle(this, callback);
  // If hasPath is false, we may disable the pref of d property, so we fallback
  // to check mD.
  return hasPath || !mD.GetAnimValue().IsEmpty();
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGPathElement::IsAttributeMapped(const nsAtom* name) const {
  return name == nsGkAtoms::d || SVGPathElementBase::IsAttributeMapped(name);
}

already_AddRefed<Path> SVGPathElement::GetOrBuildPathForMeasuring() {
  RefPtr<Path> path;
  bool success = SVGGeometryProperty::DoForComputedStyle(
      this, [&path](const ComputedStyle* s) {
        const auto& d = s->StyleSVGReset()->mD;
        if (d.IsNone()) {
          return;
        }
        path = SVGPathData::BuildPathForMeasuring(d.AsPath()._0.AsSpan());
      });
  return success ? path.forget() : mD.GetAnimValue().BuildPathForMeasuring();
}

//----------------------------------------------------------------------
// SVGGeometryElement methods

bool SVGPathElement::AttributeDefinesGeometry(const nsAtom* aName) {
  return aName == nsGkAtoms::d || aName == nsGkAtoms::pathLength;
}

bool SVGPathElement::IsMarkable() { return true; }

void SVGPathElement::GetMarkPoints(nsTArray<SVGMark>* aMarks) {
  auto callback = [aMarks](const ComputedStyle* s) {
    const nsStyleSVGReset* styleSVGReset = s->StyleSVGReset();
    if (styleSVGReset->mD.IsPath()) {
      Span<const StylePathCommand> path =
          styleSVGReset->mD.AsPath()._0.AsSpan();
      SVGPathData::GetMarkerPositioningData(path, aMarks);
    }
  };

  if (SVGGeometryProperty::DoForComputedStyle(this, callback)) {
    return;
  }

  mD.GetAnimValue().GetMarkerPositioningData(aMarks);
}

void SVGPathElement::GetAsSimplePath(SimplePath* aSimplePath) {
  aSimplePath->Reset();
  auto callback = [&](const ComputedStyle* s) {
    const nsStyleSVGReset* styleSVGReset = s->StyleSVGReset();
    if (styleSVGReset->mD.IsPath()) {
      auto pathData = styleSVGReset->mD.AsPath()._0.AsSpan();
      auto maybeRect = SVGPathToAxisAlignedRect(pathData);
      if (maybeRect.isSome()) {
        Rect r = maybeRect.value();
        aSimplePath->SetRect(r.x, r.y, r.width, r.height);
      }
    }
  };

  SVGGeometryProperty::DoForComputedStyle(this, callback);
}

already_AddRefed<Path> SVGPathElement::BuildPath(PathBuilder* aBuilder) {
  // The Moz2D PathBuilder that our SVGPathData will be using only cares about
  // the fill rule. However, in order to fulfill the requirements of the SVG
  // spec regarding zero length sub-paths when square line caps are in use,
  // SVGPathData needs to know our stroke-linecap style and, if "square", then
  // also our stroke width. See the comment for
  // ApproximateZeroLengthSubpathSquareCaps for more info.

  auto strokeLineCap = StyleStrokeLinecap::Butt;
  Float strokeWidth = 0;
  RefPtr<Path> path;

  auto callback = [&](const ComputedStyle* s) {
    const nsStyleSVG* styleSVG = s->StyleSVG();
    // Note: the path that we return may be used for hit-testing, and SVG
    // exposes hit-testing of strokes that are not actually painted. For that
    // reason we do not check for eStyleSVGPaintType_None or check the stroke
    // opacity here.
    if (styleSVG->mStrokeLinecap != StyleStrokeLinecap::Butt) {
      strokeLineCap = styleSVG->mStrokeLinecap;
      strokeWidth = SVGContentUtils::GetStrokeWidth(this, s, nullptr);
    }

    const auto& d = s->StyleSVGReset()->mD;
    if (d.IsPath()) {
      path = SVGPathData::BuildPath(d.AsPath()._0.AsSpan(), aBuilder,
                                    strokeLineCap, strokeWidth);
    }
  };

  bool success = SVGGeometryProperty::DoForComputedStyle(this, callback);
  if (success) {
    return path.forget();
  }

  // Fallback to use the d attribute if it exists.
  return mD.GetAnimValue().BuildPath(aBuilder, strokeLineCap, strokeWidth);
}

bool SVGPathElement::GetDistancesFromOriginToEndsOfVisibleSegments(
    FallibleTArray<double>* aOutput) {
  bool ret = false;
  auto callback = [&ret, aOutput](const ComputedStyle* s) {
    const auto& d = s->StyleSVGReset()->mD;
    ret = d.IsNone() ||
          SVGPathData::GetDistancesFromOriginToEndsOfVisibleSegments(
              d.AsPath()._0.AsSpan(), aOutput);
  };

  if (SVGGeometryProperty::DoForComputedStyle(this, callback)) {
    return ret;
  }

  return mD.GetAnimValue().GetDistancesFromOriginToEndsOfVisibleSegments(
      aOutput);
}

// Offset paths (including references to SVG Paths) are closed loops only if the
// final command in the path list is a closepath command ("z" or "Z"), otherwise
// they are unclosed intervals.
// https://drafts.fxtf.org/motion/#path-distance
bool SVGPathElement::IsClosedLoop() const {
  bool isClosed = false;
  auto callback = [&](const ComputedStyle* s) {
    const nsStyleSVGReset* styleSVGReset = s->StyleSVGReset();
    if (styleSVGReset->mD.IsPath()) {
      isClosed = !styleSVGReset->mD.AsPath()._0.IsEmpty() &&
                 styleSVGReset->mD.AsPath()._0.AsSpan().rbegin()->IsClosePath();
    }
  };

  const bool success = SVGGeometryProperty::DoForComputedStyle(this, callback);
  if (success) {
    return isClosed;
  }

  const SVGPathData& data = mD.GetAnimValue();
  // FIXME: Bug 1847621, we can cache this value, instead of walking through the
  // entire path again and again.
  uint32_t i = 0;
  uint32_t segType = SVGPathSeg_Binding::PATHSEG_UNKNOWN;
  while (i < data.Length()) {
    segType = SVGPathSegUtils::DecodeType(data[i++]);
    i += SVGPathSegUtils::ArgCountForType(segType);
  }
  return segType == SVGPathSeg_Binding::PATHSEG_CLOSEPATH;
}

/* static */
bool SVGPathElement::IsDPropertyChangedViaCSS(const ComputedStyle& aNewStyle,
                                              const ComputedStyle& aOldStyle) {
  return aNewStyle.StyleSVGReset()->mD != aOldStyle.StyleSVGReset()->mD;
}

}  // namespace mozilla::dom
