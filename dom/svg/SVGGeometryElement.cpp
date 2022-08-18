/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGGeometryElement.h"

#include "DOMSVGPoint.h"
#include "gfxPlatform.h"
#include "nsCOMPtr.h"
#include "SVGAnimatedLength.h"
#include "SVGCircleElement.h"
#include "SVGEllipseElement.h"
#include "SVGGeometryProperty.h"
#include "SVGPathElement.h"
#include "SVGRectElement.h"
#include "mozilla/dom/DOMPointBinding.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/SVGContentUtils.h"

using namespace mozilla::gfx;

namespace mozilla::dom {

SVGElement::NumberInfo SVGGeometryElement::sNumberInfo = {nsGkAtoms::pathLength,
                                                          0, false};

//----------------------------------------------------------------------
// Implementation

SVGGeometryElement::SVGGeometryElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGGeometryElementBase(std::move(aNodeInfo)) {}

SVGElement::NumberAttributesInfo SVGGeometryElement::GetNumberInfo() {
  return NumberAttributesInfo(&mPathLength, &sNumberInfo, 1);
}

nsresult SVGGeometryElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                          const nsAttrValue* aValue,
                                          const nsAttrValue* aOldValue,
                                          nsIPrincipal* aSubjectPrincipal,
                                          bool aNotify) {
  if (mCachedPath && aNamespaceID == kNameSpaceID_None &&
      AttributeDefinesGeometry(aName)) {
    mCachedPath = nullptr;
  }
  return SVGGeometryElementBase::AfterSetAttr(
      aNamespaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

bool SVGGeometryElement::IsNodeOfType(uint32_t aFlags) const {
  return !(aFlags & ~(eSHAPE | eUSE_TARGET));
}

bool SVGGeometryElement::AttributeDefinesGeometry(const nsAtom* aName) {
  if (aName == nsGkAtoms::pathLength) {
    return true;
  }

  // Check for SVGAnimatedLength attribute
  LengthAttributesInfo info = GetLengthInfo();
  for (uint32_t i = 0; i < info.mCount; i++) {
    if (aName == info.mInfos[i].mName) {
      return true;
    }
  }

  return false;
}

bool SVGGeometryElement::GeometryDependsOnCoordCtx() {
  // Check the SVGAnimatedLength attribute
  LengthAttributesInfo info =
      const_cast<SVGGeometryElement*>(this)->GetLengthInfo();
  for (uint32_t i = 0; i < info.mCount; i++) {
    if (info.mValues[i].GetSpecifiedUnitType() ==
        SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE) {
      return true;
    }
  }
  return false;
}

bool SVGGeometryElement::IsMarkable() { return false; }

void SVGGeometryElement::GetMarkPoints(nsTArray<SVGMark>* aMarks) {}

already_AddRefed<Path> SVGGeometryElement::GetOrBuildPath(
    const DrawTarget* aDrawTarget, FillRule aFillRule) {
  // We only cache the path if it matches the backend used for screen painting,
  // and it's not a capturing drawtarget. A capturing DT might start using the
  // the Path object on a different thread (OMTP), and we might have a data race
  // if we keep a handle to it.
  bool cacheable = aDrawTarget->GetBackendType() ==
                   gfxPlatform::GetPlatform()->GetDefaultContentBackend();

  if (cacheable && mCachedPath && mCachedPath->GetFillRule() == aFillRule &&
      aDrawTarget->GetBackendType() == mCachedPath->GetBackendType()) {
    RefPtr<Path> path(mCachedPath);
    return path.forget();
  }
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder(aFillRule);
  RefPtr<Path> path = BuildPath(builder);
  if (cacheable) {
    mCachedPath = path;
  }
  return path.forget();
}

already_AddRefed<Path> SVGGeometryElement::GetOrBuildPathForMeasuring() {
  RefPtr<DrawTarget> drawTarget =
      gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
  FillRule fillRule = mCachedPath ? mCachedPath->GetFillRule() : GetFillRule();
  return GetOrBuildPath(drawTarget, fillRule);
}

// This helper is currently identical to GetOrBuildPathForMeasuring.
// We keep it a separate method because for length measuring purpose,
// fillRule isn't really needed. Derived class (e.g. SVGPathElement)
// may override GetOrBuildPathForMeasuring() to ignore fillRule. And
// GetOrBuildPathForMeasuring() itself may be modified in the future.
already_AddRefed<Path> SVGGeometryElement::GetOrBuildPathForHitTest() {
  RefPtr<DrawTarget> drawTarget =
      gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
  FillRule fillRule = mCachedPath ? mCachedPath->GetFillRule() : GetFillRule();
  return GetOrBuildPath(drawTarget, fillRule);
}

bool SVGGeometryElement::IsGeometryChangedViaCSS(
    ComputedStyle const& aNewStyle, ComputedStyle const& aOldStyle) const {
  nsAtom* name = NodeInfo()->NameAtom();
  if (name == nsGkAtoms::rect) {
    return SVGRectElement::IsLengthChangedViaCSS(aNewStyle, aOldStyle);
  }
  if (name == nsGkAtoms::circle) {
    return SVGCircleElement::IsLengthChangedViaCSS(aNewStyle, aOldStyle);
  }
  if (name == nsGkAtoms::ellipse) {
    return SVGEllipseElement::IsLengthChangedViaCSS(aNewStyle, aOldStyle);
  }
  if (name == nsGkAtoms::path) {
    return StaticPrefs::layout_css_d_property_enabled() &&
           SVGPathElement::IsDPropertyChangedViaCSS(aNewStyle, aOldStyle);
  }
  return false;
}

FillRule SVGGeometryElement::GetFillRule() {
  FillRule fillRule =
      FillRule::FILL_WINDING;  // Equivalent to StyleFillRule::Nonzero

  bool res = SVGGeometryProperty::DoForComputedStyle(
      this, [&](const ComputedStyle* s) {
        const auto* styleSVG = s->StyleSVG();

        MOZ_ASSERT(styleSVG->mFillRule == StyleFillRule::Nonzero ||
                   styleSVG->mFillRule == StyleFillRule::Evenodd);

        if (styleSVG->mFillRule == StyleFillRule::Evenodd) {
          fillRule = FillRule::FILL_EVEN_ODD;
        }
      });

  if (!res) {
    NS_WARNING("Couldn't get ComputedStyle for content in GetFillRule");
  }

  return fillRule;
}

static Point GetPointFrom(const DOMPointInit& aPoint) {
  return Point(aPoint.mX, aPoint.mY);
}

bool SVGGeometryElement::IsPointInFill(const DOMPointInit& aPoint) {
  // d is a presentation attribute, so make sure style is up to date:
  FlushStyleIfNeeded();

  RefPtr<Path> path = GetOrBuildPathForHitTest();
  if (!path) {
    return false;
  }

  auto point = GetPointFrom(aPoint);
  return path->ContainsPoint(point, {});
}

bool SVGGeometryElement::IsPointInStroke(const DOMPointInit& aPoint) {
  // stroke-* attributes and the d attribute are presentation attributes, so we
  // flush the layout before building the path.
  if (nsCOMPtr<Document> doc = GetComposedDoc()) {
    doc->FlushPendingNotifications(FlushType::Layout);
  }

  RefPtr<Path> path = GetOrBuildPathForHitTest();
  if (!path) {
    return false;
  }

  auto point = GetPointFrom(aPoint);
  bool res = false;
  SVGGeometryProperty::DoForComputedStyle(this, [&](const ComputedStyle* s) {
    // Per spec, we should take vector-effect into account.
    if (s->StyleSVGReset()->HasNonScalingStroke()) {
      auto mat = SVGContentUtils::GetCTM(this, true);
      if (mat.HasNonTranslation()) {
        // We have non-scaling-stroke as well as a non-translation transform.
        // We should transform the path first then apply the stroke on the
        // transformed path to preserve the stroke-width.
        RefPtr<PathBuilder> builder = path->TransformedCopyToBuilder(mat);

        path = builder->Finish();
        point = mat.TransformPoint(point);
      }
    }

    SVGContentUtils::AutoStrokeOptions strokeOptions;
    SVGContentUtils::GetStrokeOptions(&strokeOptions, this, s, nullptr);

    res = path->StrokeContainsPoint(strokeOptions, point, {});
  });

  return res;
}

float SVGGeometryElement::GetTotalLengthForBinding() {
  // d is a presentation attribute, so make sure style is up to date:
  FlushStyleIfNeeded();
  return GetTotalLength();
}

already_AddRefed<DOMSVGPoint> SVGGeometryElement::GetPointAtLength(
    float distance, ErrorResult& rv) {
  // d is a presentation attribute, so make sure style is up to date:
  FlushStyleIfNeeded();
  RefPtr<Path> path = GetOrBuildPathForMeasuring();
  if (!path) {
    rv.ThrowInvalidStateError("No path available for measuring");
    return nullptr;
  }

  RefPtr<DOMSVGPoint> point = new DOMSVGPoint(path->ComputePointAtLength(
      clamped(distance, 0.f, path->ComputeLength())));
  return point.forget();
}

float SVGGeometryElement::GetPathLengthScale(PathLengthScaleForType aFor) {
  MOZ_ASSERT(aFor == eForTextPath || aFor == eForStroking, "Unknown enum");
  if (mPathLength.IsExplicitlySet()) {
    float authorsPathLengthEstimate = mPathLength.GetAnimValue();
    if (authorsPathLengthEstimate > 0) {
      RefPtr<Path> path = GetOrBuildPathForMeasuring();
      if (!path) {
        // The path is empty or invalid so its length must be zero and
        // we know that 0 / authorsPathLengthEstimate = 0.
        return 0.0;
      }
      if (aFor == eForTextPath) {
        // For textPath, a transform on the referenced path affects the
        // textPath layout, so when calculating the actual path length
        // we need to take that into account.
        gfxMatrix matrix = PrependLocalTransformsTo(gfxMatrix());
        if (!matrix.IsIdentity()) {
          RefPtr<PathBuilder> builder =
              path->TransformedCopyToBuilder(ToMatrix(matrix));
          path = builder->Finish();
        }
      }
      return path->ComputeLength() / authorsPathLengthEstimate;
    }
  }
  return 1.0;
}

already_AddRefed<DOMSVGAnimatedNumber> SVGGeometryElement::PathLength() {
  return mPathLength.ToDOMAnimatedNumber(this);
}

float SVGGeometryElement::GetTotalLength() {
  RefPtr<Path> flat = GetOrBuildPathForMeasuring();
  return flat ? flat->ComputeLength() : 0.f;
}

void SVGGeometryElement::FlushStyleIfNeeded() {
  // Note: we still can set d property on other elements which don't have d
  // attribute, but we don't look at the d property on them, so here we only
  // care about the element with d attribute, i.e. SVG path element.
  if (GetPathDataAttrName() != nsGkAtoms::d ||
      !StaticPrefs::layout_css_d_property_enabled()) {
    return;
  }

  RefPtr<Document> doc = GetComposedDoc();
  if (!doc) {
    return;
  }

  doc->FlushPendingNotifications(FlushType::Style);
}

}  // namespace mozilla::dom
