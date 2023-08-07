/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGPATHELEMENT_H_
#define DOM_SVG_SVGPATHELEMENT_H_

#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "SVGAnimatedPathSegList.h"
#include "SVGGeometryElement.h"
#include "DOMSVGPathSeg.h"

nsresult NS_NewSVGPathElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

using SVGPathElementBase = SVGGeometryElement;

class SVGPathElement final : public SVGPathElementBase {
  using Path = mozilla::gfx::Path;

 protected:
  friend nsresult(::NS_NewSVGPathElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;
  explicit SVGPathElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  void GetAsSimplePath(SimplePath* aSimplePath) override;

 public:
  NS_DECL_ADDSIZEOFEXCLUDINGTHIS

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* name) const override;

  // SVGSVGElement methods:
  bool HasValidDimensions() const override;

  // SVGGeometryElement methods:
  bool AttributeDefinesGeometry(const nsAtom* aName) override;
  bool IsMarkable() override;
  void GetMarkPoints(nsTArray<SVGMark>* aMarks) override;
  /*
   * Note: This function maps d attribute to CSS d property, and we don't flush
   * style in this function because some callers don't need it, so if the caller
   * needs style to be flushed (e.g. DOM APIs), the caller should flush style
   * before calling this.
   */
  already_AddRefed<Path> BuildPath(PathBuilder* aBuilder) override;

  /**
   * This returns a path without the extra little line segments that
   * ApproximateZeroLengthSubpathSquareCaps can insert if we have square-caps.
   * See the comment for that function for more info on that.
   *
   * Note: This function maps d attribute to CSS d property, and we don't flush
   * style in this function because some callers don't need it, so if the caller
   * needs style to be flushed (e.g. DOM APIs), the caller should flush style
   * before calling this.
   */
  already_AddRefed<Path> GetOrBuildPathForMeasuring() override;

  bool GetDistancesFromOriginToEndsOfVisibleSegments(
      FallibleTArray<double>* aOutput) override;

  bool IsClosedLoop() const override;

  // nsIContent interface
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  SVGAnimatedPathSegList* GetAnimPathSegList() override { return &mD; }

  nsStaticAtom* GetPathDataAttrName() const override { return nsGkAtoms::d; }

  // WebIDL
  MOZ_CAN_RUN_SCRIPT uint32_t GetPathSegAtLength(float distance);
  already_AddRefed<DOMSVGPathSegClosePath> CreateSVGPathSegClosePath();
  already_AddRefed<DOMSVGPathSegMovetoAbs> CreateSVGPathSegMovetoAbs(float x,
                                                                     float y);
  already_AddRefed<DOMSVGPathSegMovetoRel> CreateSVGPathSegMovetoRel(float x,
                                                                     float y);
  already_AddRefed<DOMSVGPathSegLinetoAbs> CreateSVGPathSegLinetoAbs(float x,
                                                                     float y);
  already_AddRefed<DOMSVGPathSegLinetoRel> CreateSVGPathSegLinetoRel(float x,
                                                                     float y);
  already_AddRefed<DOMSVGPathSegCurvetoCubicAbs>
  CreateSVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1,
                                  float x2, float y2);
  already_AddRefed<DOMSVGPathSegCurvetoCubicRel>
  CreateSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1,
                                  float x2, float y2);
  already_AddRefed<DOMSVGPathSegCurvetoQuadraticAbs>
  CreateSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1);
  already_AddRefed<DOMSVGPathSegCurvetoQuadraticRel>
  CreateSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1);
  already_AddRefed<DOMSVGPathSegArcAbs> CreateSVGPathSegArcAbs(
      float x, float y, float r1, float r2, float angle, bool largeArcFlag,
      bool sweepFlag);
  already_AddRefed<DOMSVGPathSegArcRel> CreateSVGPathSegArcRel(
      float x, float y, float r1, float r2, float angle, bool largeArcFlag,
      bool sweepFlag);
  already_AddRefed<DOMSVGPathSegLinetoHorizontalAbs>
  CreateSVGPathSegLinetoHorizontalAbs(float x);
  already_AddRefed<DOMSVGPathSegLinetoHorizontalRel>
  CreateSVGPathSegLinetoHorizontalRel(float x);
  already_AddRefed<DOMSVGPathSegLinetoVerticalAbs>
  CreateSVGPathSegLinetoVerticalAbs(float y);
  already_AddRefed<DOMSVGPathSegLinetoVerticalRel>
  CreateSVGPathSegLinetoVerticalRel(float y);
  already_AddRefed<DOMSVGPathSegCurvetoCubicSmoothAbs>
  CreateSVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2);
  already_AddRefed<DOMSVGPathSegCurvetoCubicSmoothRel>
  CreateSVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2);
  already_AddRefed<DOMSVGPathSegCurvetoQuadraticSmoothAbs>
  CreateSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y);
  already_AddRefed<DOMSVGPathSegCurvetoQuadraticSmoothRel>
  CreateSVGPathSegCurvetoQuadraticSmoothRel(float x, float y);
  already_AddRefed<DOMSVGPathSegList> PathSegList();
  already_AddRefed<DOMSVGPathSegList> AnimatedPathSegList();

  static bool IsDPropertyChangedViaCSS(const ComputedStyle& aNewStyle,
                                       const ComputedStyle& aOldStyle);

 protected:
  SVGAnimatedPathSegList mD;
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGPATHELEMENT_H_
