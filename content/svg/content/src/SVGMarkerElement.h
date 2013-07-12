/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGMarkerElement_h
#define mozilla_dom_SVGMarkerElement_h

#include "gfxMatrix.h"
#include "nsSVGAngle.h"
#include "nsSVGEnum.h"
#include "nsSVGLength2.h"
#include "nsSVGViewBox.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "nsSVGElement.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/SVGAnimatedEnumeration.h"

class nsSVGMarkerFrame;

nsresult NS_NewSVGMarkerElement(nsIContent **aResult,
                                already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

// Marker Unit Types
static const unsigned short SVG_MARKERUNITS_UNKNOWN         = 0;
static const unsigned short SVG_MARKERUNITS_USERSPACEONUSE = 1;
static const unsigned short SVG_MARKERUNITS_STROKEWIDTH    = 2;

// Marker Orientation Types
static const unsigned short SVG_MARKER_ORIENT_UNKNOWN      = 0;
static const unsigned short SVG_MARKER_ORIENT_AUTO         = 1;
static const unsigned short SVG_MARKER_ORIENT_ANGLE        = 2;

class nsSVGOrientType
{
public:
  nsSVGOrientType()
   : mAnimVal(SVG_MARKER_ORIENT_ANGLE),
     mBaseVal(SVG_MARKER_ORIENT_ANGLE) {}

  nsresult SetBaseValue(uint16_t aValue,
                        nsSVGElement *aSVGElement);

  // XXX FIXME like https://bugzilla.mozilla.org/show_bug.cgi?id=545550 but
  // without adding an mIsAnimated member...?
  void SetBaseValue(uint16_t aValue)
    { mAnimVal = mBaseVal = uint8_t(aValue); }
  // no need to notify, since nsSVGAngle does that
  void SetAnimValue(uint16_t aValue)
    { mAnimVal = uint8_t(aValue); }

  uint16_t GetBaseValue() const
    { return mBaseVal; }
  uint16_t GetAnimValue() const
    { return mAnimVal; }

  already_AddRefed<SVGAnimatedEnumeration>
    ToDOMAnimatedEnum(nsSVGElement* aSVGElement);

private:
  nsSVGEnumValue mAnimVal;
  nsSVGEnumValue mBaseVal;

  struct DOMAnimatedEnum MOZ_FINAL : public SVGAnimatedEnumeration
  {
    DOMAnimatedEnum(nsSVGOrientType* aVal,
                    nsSVGElement *aSVGElement)
      : SVGAnimatedEnumeration(aSVGElement)
      , mVal(aVal)
    {}

    nsSVGOrientType *mVal; // kept alive because it belongs to content

    using mozilla::dom::SVGAnimatedEnumeration::SetBaseVal;
    virtual uint16_t BaseVal() MOZ_OVERRIDE
    {
      return mVal->GetBaseValue();
    }
    virtual void SetBaseVal(uint16_t aBaseVal, ErrorResult& aRv) MOZ_OVERRIDE
    {
      aRv = mVal->SetBaseValue(aBaseVal, mSVGElement);
    }
    virtual uint16_t AnimVal() MOZ_OVERRIDE
    {
      return mVal->GetAnimValue();
    }
  };
};

typedef nsSVGElement SVGMarkerElementBase;

class SVGMarkerElement : public SVGMarkerElementBase
{
  friend class ::nsSVGMarkerFrame;

protected:
  friend nsresult (::NS_NewSVGMarkerElement(nsIContent **aResult,
                                            already_AddRefed<nsINodeInfo> aNodeInfo));
  SVGMarkerElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx,
                             JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const MOZ_OVERRIDE;

  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) MOZ_OVERRIDE;

  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const MOZ_OVERRIDE;

  // public helpers
  gfxMatrix GetMarkerTransform(float aStrokeWidth,
                               float aX, float aY, float aAutoAngle);
  nsSVGViewBoxRect GetViewBoxRect();
  gfxMatrix GetViewBoxTransform();

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  nsSVGOrientType* GetOrientType() { return &mOrientType; }

  // Returns the value of svg.marker-improvements.enabled.
  static bool MarkerImprovementsPrefEnabled();

  // WebIDL
  already_AddRefed<SVGAnimatedRect> ViewBox();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();
  already_AddRefed<SVGAnimatedLength> RefX();
  already_AddRefed<SVGAnimatedLength> RefY();
  already_AddRefed<SVGAnimatedEnumeration> MarkerUnits();
  already_AddRefed<SVGAnimatedLength> MarkerWidth();
  already_AddRefed<SVGAnimatedLength> MarkerHeight();
  already_AddRefed<SVGAnimatedEnumeration> OrientType();
  already_AddRefed<SVGAnimatedAngle> OrientAngle();
  void SetOrientToAuto();
  void SetOrientToAngle(SVGAngle& angle, ErrorResult& rv);

protected:

  virtual bool ParseAttribute(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAString& aValue,
                                nsAttrValue& aResult) MOZ_OVERRIDE;

  void SetParentCoordCtxProvider(SVGSVGElement *aContext);

  virtual LengthAttributesInfo GetLengthInfo() MOZ_OVERRIDE;
  virtual AngleAttributesInfo GetAngleInfo() MOZ_OVERRIDE;
  virtual EnumAttributesInfo GetEnumInfo() MOZ_OVERRIDE;
  virtual nsSVGViewBox *GetViewBox() MOZ_OVERRIDE;
  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio() MOZ_OVERRIDE;

  enum { REFX, REFY, MARKERWIDTH, MARKERHEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { MARKERUNITS };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sUnitsMap[];
  static EnumInfo sEnumInfo[1];

  enum { ORIENT };
  nsSVGAngle mAngleAttributes[1];
  static AngleInfo sAngleInfo[1];

  nsSVGViewBox             mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;

  // derived properties (from 'orient') handled separately
  nsSVGOrientType                        mOrientType;

  SVGSVGElement                         *mCoordCtx;
  nsAutoPtr<gfxMatrix>                   mViewBoxToViewportTransform;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGMarkerElement_h
