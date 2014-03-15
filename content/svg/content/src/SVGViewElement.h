/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGViewElement_h
#define mozilla_dom_SVGViewElement_h

#include "nsSVGElement.h"
#include "nsSVGEnum.h"
#include "nsSVGViewBox.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGStringList.h"

static const unsigned short SVG_ZOOMANDPAN_UNKNOWN = 0;
static const unsigned short SVG_ZOOMANDPAN_DISABLE = 1;
static const unsigned short SVG_ZOOMANDPAN_MAGNIFY = 2;

typedef nsSVGElement SVGViewElementBase;

class nsSVGOuterSVGFrame;

nsresult NS_NewSVGViewElement(nsIContent **aResult,
                              already_AddRefed<nsINodeInfo>&& aNodeInfo);

namespace mozilla {
class SVGFragmentIdentifier;

namespace dom {
class SVGSVGElement;

class SVGViewElement : public SVGViewElementBase
{
protected:
  friend class mozilla::SVGFragmentIdentifier;
  friend class SVGSVGElement;
  friend class ::nsSVGOuterSVGFrame;
  SVGViewElement(already_AddRefed<nsINodeInfo>& aNodeInfo);
  friend nsresult (::NS_NewSVGViewElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo>&& aNodeInfo));
  virtual JSObject* WrapNode(JSContext *cx,
                             JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

public:
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL
  uint16_t ZoomAndPan() { return mEnumAttributes[ZOOMANDPAN].GetAnimValue(); }
  void SetZoomAndPan(uint16_t aZoomAndPan, ErrorResult& rv);
  already_AddRefed<SVGAnimatedRect> ViewBox();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();
  already_AddRefed<DOMSVGStringList> ViewTarget();

private:

  // nsSVGElement overrides

  virtual EnumAttributesInfo GetEnumInfo() MOZ_OVERRIDE;

  enum { ZOOMANDPAN };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sZoomAndPanMap[];
  static EnumInfo sEnumInfo[1];

  virtual nsSVGViewBox *GetViewBox() MOZ_OVERRIDE;
  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio() MOZ_OVERRIDE;

  nsSVGViewBox                   mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;

  virtual StringListAttributesInfo GetStringListInfo() MOZ_OVERRIDE;

  enum { VIEW_TARGET };
  SVGStringList mStringListAttributes[1];
  static StringListInfo sStringListInfo[1];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGViewElement_h
