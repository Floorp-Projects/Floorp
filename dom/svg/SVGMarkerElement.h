/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGMarkerElement_h
#define mozilla_dom_SVGMarkerElement_h

#include "nsAutoPtr.h"
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
                                already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

// Marker Unit Types
static const unsigned short SVG_MARKERUNITS_UNKNOWN         = 0;
static const unsigned short SVG_MARKERUNITS_USERSPACEONUSE = 1;
static const unsigned short SVG_MARKERUNITS_STROKEWIDTH    = 2;

// Marker Orientation Types
static const unsigned short SVG_MARKER_ORIENT_UNKNOWN            = 0;
static const unsigned short SVG_MARKER_ORIENT_AUTO               = 1;
static const unsigned short SVG_MARKER_ORIENT_ANGLE              = 2;
static const unsigned short SVG_MARKER_ORIENT_AUTO_START_REVERSE = 3;

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

  // we want to avoid exposing SVG_MARKER_ORIENT_AUTO_START_REVERSE to
  // Web content
  uint16_t GetBaseValue() const
    { return mAnimVal == SVG_MARKER_ORIENT_AUTO_START_REVERSE ?
               SVG_MARKER_ORIENT_UNKNOWN : mBaseVal; }
  uint16_t GetAnimValue() const
    { return mAnimVal == SVG_MARKER_ORIENT_AUTO_START_REVERSE ?
               SVG_MARKER_ORIENT_UNKNOWN : mAnimVal; }
  uint16_t GetAnimValueInternal() const
    { return mAnimVal; }

  already_AddRefed<SVGAnimatedEnumeration>
    ToDOMAnimatedEnum(nsSVGElement* aSVGElement);

private:
  nsSVGEnumValue mAnimVal;
  nsSVGEnumValue mBaseVal;

  struct DOMAnimatedEnum final : public SVGAnimatedEnumeration
  {
    DOMAnimatedEnum(nsSVGOrientType* aVal,
                    nsSVGElement *aSVGElement)
      : SVGAnimatedEnumeration(aSVGElement)
      , mVal(aVal)
    {}

    nsSVGOrientType *mVal; // kept alive because it belongs to content

    using mozilla::dom::SVGAnimatedEnumeration::SetBaseVal;
    virtual uint16_t BaseVal() override
    {
      return mVal->GetBaseValue();
    }
    virtual void SetBaseVal(uint16_t aBaseVal, ErrorResult& aRv) override
    {
      aRv = mVal->SetBaseValue(aBaseVal, mSVGElement);
    }
    virtual uint16_t AnimVal() override
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
                                            already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGMarkerElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const override;

  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) override;

  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const override;

  // public helpers
  gfx::Matrix GetMarkerTransform(float aStrokeWidth,
                                 float aX, float aY, float aAutoAngle,
                                 bool aIsStart);
  nsSVGViewBoxRect GetViewBoxRect();
  gfx::Matrix GetViewBoxTransform();

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

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
                                nsAttrValue& aResult) override;

  void SetParentCoordCtxProvider(SVGSVGElement *aContext);

  virtual LengthAttributesInfo GetLengthInfo() override;
  virtual AngleAttributesInfo GetAngleInfo() override;
  virtual EnumAttributesInfo GetEnumInfo() override;
  virtual nsSVGViewBox *GetViewBox() override;
  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio() override;

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
  nsAutoPtr<gfx::Matrix>                 mViewBoxToViewportTransform;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGMarkerElement_h
