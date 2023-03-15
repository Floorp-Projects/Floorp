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

  if (StaticPrefs::layout_css_d_property_enabled()) {
    FlushStyleIfNeeded();
    if (SVGGeometryProperty::DoForComputedStyle(this, callback)) {
      return seg;
    }
  }
  return mD.GetAnimValue().GetPathSegAtLength(distance);
}

already_AddRefed<DOMSVGPathSegClosePath>
SVGPathElement::CreateSVGPathSegClosePath() {
  RefPtr<DOMSVGPathSegClosePath> pathSeg = new DOMSVGPathSegClosePath();
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegMovetoAbs>
SVGPathElement::CreateSVGPathSegMovetoAbs(float x, float y) {
  RefPtr<DOMSVGPathSegMovetoAbs> pathSeg = new DOMSVGPathSegMovetoAbs(x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegMovetoRel>
SVGPathElement::CreateSVGPathSegMovetoRel(float x, float y) {
  RefPtr<DOMSVGPathSegMovetoRel> pathSeg = new DOMSVGPathSegMovetoRel(x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegLinetoAbs>
SVGPathElement::CreateSVGPathSegLinetoAbs(float x, float y) {
  RefPtr<DOMSVGPathSegLinetoAbs> pathSeg = new DOMSVGPathSegLinetoAbs(x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegLinetoRel>
SVGPathElement::CreateSVGPathSegLinetoRel(float x, float y) {
  RefPtr<DOMSVGPathSegLinetoRel> pathSeg = new DOMSVGPathSegLinetoRel(x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegCurvetoCubicAbs>
SVGPathElement::CreateSVGPathSegCurvetoCubicAbs(float x, float y, float x1,
                                                float y1, float x2, float y2) {
  // Note that we swap from DOM API argument order to the argument order used
  // in the <path> element's 'd' attribute (i.e. we put the arguments for the
  // end point of the segment last instead of first).
  RefPtr<DOMSVGPathSegCurvetoCubicAbs> pathSeg =
      new DOMSVGPathSegCurvetoCubicAbs(x1, y1, x2, y2, x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegCurvetoCubicRel>
SVGPathElement::CreateSVGPathSegCurvetoCubicRel(float x, float y, float x1,
                                                float y1, float x2, float y2) {
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  RefPtr<DOMSVGPathSegCurvetoCubicRel> pathSeg =
      new DOMSVGPathSegCurvetoCubicRel(x1, y1, x2, y2, x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegCurvetoQuadraticAbs>
SVGPathElement::CreateSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1,
                                                    float y1) {
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  RefPtr<DOMSVGPathSegCurvetoQuadraticAbs> pathSeg =
      new DOMSVGPathSegCurvetoQuadraticAbs(x1, y1, x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegCurvetoQuadraticRel>
SVGPathElement::CreateSVGPathSegCurvetoQuadraticRel(float x, float y, float x1,
                                                    float y1) {
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  RefPtr<DOMSVGPathSegCurvetoQuadraticRel> pathSeg =
      new DOMSVGPathSegCurvetoQuadraticRel(x1, y1, x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegArcAbs> SVGPathElement::CreateSVGPathSegArcAbs(
    float x, float y, float r1, float r2, float angle, bool largeArcFlag,
    bool sweepFlag) {
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  RefPtr<DOMSVGPathSegArcAbs> pathSeg =
      new DOMSVGPathSegArcAbs(r1, r2, angle, largeArcFlag, sweepFlag, x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegArcRel> SVGPathElement::CreateSVGPathSegArcRel(
    float x, float y, float r1, float r2, float angle, bool largeArcFlag,
    bool sweepFlag) {
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  RefPtr<DOMSVGPathSegArcRel> pathSeg =
      new DOMSVGPathSegArcRel(r1, r2, angle, largeArcFlag, sweepFlag, x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegLinetoHorizontalAbs>
SVGPathElement::CreateSVGPathSegLinetoHorizontalAbs(float x) {
  RefPtr<DOMSVGPathSegLinetoHorizontalAbs> pathSeg =
      new DOMSVGPathSegLinetoHorizontalAbs(x);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegLinetoHorizontalRel>
SVGPathElement::CreateSVGPathSegLinetoHorizontalRel(float x) {
  RefPtr<DOMSVGPathSegLinetoHorizontalRel> pathSeg =
      new DOMSVGPathSegLinetoHorizontalRel(x);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegLinetoVerticalAbs>
SVGPathElement::CreateSVGPathSegLinetoVerticalAbs(float y) {
  RefPtr<DOMSVGPathSegLinetoVerticalAbs> pathSeg =
      new DOMSVGPathSegLinetoVerticalAbs(y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegLinetoVerticalRel>
SVGPathElement::CreateSVGPathSegLinetoVerticalRel(float y) {
  RefPtr<DOMSVGPathSegLinetoVerticalRel> pathSeg =
      new DOMSVGPathSegLinetoVerticalRel(y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegCurvetoCubicSmoothAbs>
SVGPathElement::CreateSVGPathSegCurvetoCubicSmoothAbs(float x, float y,
                                                      float x2, float y2) {
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  RefPtr<DOMSVGPathSegCurvetoCubicSmoothAbs> pathSeg =
      new DOMSVGPathSegCurvetoCubicSmoothAbs(x2, y2, x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegCurvetoCubicSmoothRel>
SVGPathElement::CreateSVGPathSegCurvetoCubicSmoothRel(float x, float y,
                                                      float x2, float y2) {
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  RefPtr<DOMSVGPathSegCurvetoCubicSmoothRel> pathSeg =
      new DOMSVGPathSegCurvetoCubicSmoothRel(x2, y2, x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegCurvetoQuadraticSmoothAbs>
SVGPathElement::CreateSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y) {
  RefPtr<DOMSVGPathSegCurvetoQuadraticSmoothAbs> pathSeg =
      new DOMSVGPathSegCurvetoQuadraticSmoothAbs(x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegCurvetoQuadraticSmoothRel>
SVGPathElement::CreateSVGPathSegCurvetoQuadraticSmoothRel(float x, float y) {
  RefPtr<DOMSVGPathSegCurvetoQuadraticSmoothRel> pathSeg =
      new DOMSVGPathSegCurvetoQuadraticSmoothRel(x, y);
  return pathSeg.forget();
}

// FIXME: This API is enabled only if dom.svg.pathSeg.enabled is true. This
// preference is off by default in Bug 1388931, and will be dropped later.
// So we are not planning to map d property for this API.
already_AddRefed<DOMSVGPathSegList> SVGPathElement::PathSegList() {
  return DOMSVGPathSegList::GetDOMWrapper(mD.GetBaseValKey(), this, false);
}

// FIXME: This API is enabled only if dom.svg.pathSeg.enabled is true. This
// preference is off by default in Bug 1388931, and will be dropped later.
// So we are not planning to map d property for this API.
already_AddRefed<DOMSVGPathSegList> SVGPathElement::AnimatedPathSegList() {
  return DOMSVGPathSegList::GetDOMWrapper(mD.GetAnimValKey(), this, true);
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
  static const MappedAttributeEntry* const map[] = {sMarkersMap};

  return FindAttributeDependence(name, map) ||
         (StaticPrefs::layout_css_d_property_enabled() &&
          name == nsGkAtoms::d) ||
         SVGPathElementBase::IsAttributeMapped(name);
}

already_AddRefed<Path> SVGPathElement::GetOrBuildPathForMeasuring() {
  if (!StaticPrefs::layout_css_d_property_enabled()) {
    return mD.GetAnimValue().BuildPathForMeasuring();
  }

  // FIXME: Bug 1715387, the IDL methods should flush style, but internal
  // callers shouldn't. We have to make sure we flush the style well from the
  // caller.

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

  if (StaticPrefs::layout_css_d_property_enabled() &&
      SVGGeometryProperty::DoForComputedStyle(this, callback)) {
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
  const bool useDProperty = StaticPrefs::layout_css_d_property_enabled();

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

    if (!useDProperty) {
      return;
    }

    const auto& d = s->StyleSVGReset()->mD;
    if (d.IsPath()) {
      path = SVGPathData::BuildPath(d.AsPath()._0.AsSpan(), aBuilder,
                                    strokeLineCap, strokeWidth);
    }
  };

  bool success = SVGGeometryProperty::DoForComputedStyle(this, callback);
  if (success && useDProperty) {
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

  if (StaticPrefs::layout_css_d_property_enabled() &&
      SVGGeometryProperty::DoForComputedStyle(this, callback)) {
    return ret;
  }

  return mD.GetAnimValue().GetDistancesFromOriginToEndsOfVisibleSegments(
      aOutput);
}

/* static */
bool SVGPathElement::IsDPropertyChangedViaCSS(const ComputedStyle& aNewStyle,
                                              const ComputedStyle& aOldStyle) {
  return aNewStyle.StyleSVGReset()->mD != aOldStyle.StyleSVGReset()->mD;
}

}  // namespace mozilla::dom
