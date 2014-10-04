/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGPathElement_h
#define mozilla_dom_SVGPathElement_h

#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "nsSVGNumber2.h"
#include "nsSVGPathGeometryElement.h"
#include "SVGAnimatedPathSegList.h"
#include "DOMSVGPathSeg.h"

nsresult NS_NewSVGPathElement(nsIContent **aResult,
                              already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

typedef nsSVGPathGeometryElement SVGPathElementBase;

namespace mozilla {

class nsISVGPoint;

namespace dom {

class SVGPathElement MOZ_FINAL : public SVGPathElementBase
{
friend class nsSVGPathFrame;

  typedef mozilla::gfx::Path Path;

protected:
  friend nsresult (::NS_NewSVGPathElement(nsIContent **aResult,
                                          already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  virtual JSObject* WrapNode(JSContext *cx) MOZ_OVERRIDE;
  explicit SVGPathElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

public:
  // DOM memory reporter participant
  NS_DECL_SIZEOF_EXCLUDING_THIS

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const MOZ_OVERRIDE;

  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const MOZ_OVERRIDE;

  // nsSVGPathGeometryElement methods:
  virtual bool AttributeDefinesGeometry(const nsIAtom *aName) MOZ_OVERRIDE;
  virtual bool IsMarkable() MOZ_OVERRIDE;
  virtual void GetMarkPoints(nsTArray<nsSVGMark> *aMarks) MOZ_OVERRIDE;
  virtual TemporaryRef<Path> BuildPath(PathBuilder* aBuilder) MOZ_OVERRIDE;

  /**
   * This returns a path without the extra little line segments that
   * ApproximateZeroLengthSubpathSquareCaps can insert if we have square-caps.
   * See the comment for that function for more info on that.
   */
  virtual TemporaryRef<Path>
    GetPathForLengthOrPositionMeasuring() MOZ_OVERRIDE;

  // nsIContent interface
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  virtual SVGAnimatedPathSegList* GetAnimPathSegList() MOZ_OVERRIDE {
    return &mD;
  }

  virtual nsIAtom* GetPathDataAttrName() const MOZ_OVERRIDE {
    return nsGkAtoms::d;
  }

  enum PathLengthScaleForType {
    eForTextPath,
    eForStroking
  };

  /**
   * Gets the ratio of the actual path length to the content author's estimated
   * length (as provided by the <path> element's 'pathLength' attribute). This
   * is used to scale stroke dashing, and to scale offsets along a textPath.
   */
  float GetPathLengthScale(PathLengthScaleForType aFor);

  // WebIDL
  already_AddRefed<SVGAnimatedNumber> PathLength();
  float GetTotalLength();
  already_AddRefed<nsISVGPoint> GetPointAtLength(float distance, ErrorResult& rv);
  uint32_t GetPathSegAtLength(float distance);
  already_AddRefed<DOMSVGPathSegClosePath> CreateSVGPathSegClosePath();
  already_AddRefed<DOMSVGPathSegMovetoAbs> CreateSVGPathSegMovetoAbs(float x, float y);
  already_AddRefed<DOMSVGPathSegMovetoRel> CreateSVGPathSegMovetoRel(float x, float y);
  already_AddRefed<DOMSVGPathSegLinetoAbs> CreateSVGPathSegLinetoAbs(float x, float y);
  already_AddRefed<DOMSVGPathSegLinetoRel> CreateSVGPathSegLinetoRel(float x, float y);
  already_AddRefed<DOMSVGPathSegCurvetoCubicAbs>
    CreateSVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2);
  already_AddRefed<DOMSVGPathSegCurvetoCubicRel>
    CreateSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2);
  already_AddRefed<DOMSVGPathSegCurvetoQuadraticAbs>
    CreateSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1);
  already_AddRefed<DOMSVGPathSegCurvetoQuadraticRel>
    CreateSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1);
  already_AddRefed<DOMSVGPathSegArcAbs>
    CreateSVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag);
  already_AddRefed<DOMSVGPathSegArcRel>
    CreateSVGPathSegArcRel(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag);
  already_AddRefed<DOMSVGPathSegLinetoHorizontalAbs> CreateSVGPathSegLinetoHorizontalAbs(float x);
  already_AddRefed<DOMSVGPathSegLinetoHorizontalRel> CreateSVGPathSegLinetoHorizontalRel(float x);
  already_AddRefed<DOMSVGPathSegLinetoVerticalAbs> CreateSVGPathSegLinetoVerticalAbs(float y);
  already_AddRefed<DOMSVGPathSegLinetoVerticalRel> CreateSVGPathSegLinetoVerticalRel(float y);
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

protected:

  // nsSVGElement method
  virtual NumberAttributesInfo GetNumberInfo() MOZ_OVERRIDE;

  SVGAnimatedPathSegList mD;
  nsSVGNumber2 mPathLength;
  static NumberInfo sNumberInfo;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGPathElement_h
