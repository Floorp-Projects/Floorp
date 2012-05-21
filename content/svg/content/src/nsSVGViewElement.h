/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGVIEWELEMENT_H__
#define __NS_SVGVIEWELEMENT_H__

#include "nsIDOMSVGViewElement.h"
#include "nsIDOMSVGFitToViewBox.h"
#include "nsIDOMSVGZoomAndPan.h"
#include "nsSVGElement.h"
#include "nsSVGEnum.h"
#include "nsSVGViewBox.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGStringList.h"

namespace mozilla {
  class SVGFragmentIdentifier;
}

typedef nsSVGElement nsSVGViewElementBase;

class nsSVGViewElement : public nsSVGViewElementBase,
                         public nsIDOMSVGViewElement,
                         public nsIDOMSVGFitToViewBox,
                         public nsIDOMSVGZoomAndPan
{
  friend class mozilla::SVGFragmentIdentifier;
  friend nsresult NS_NewSVGViewElement(nsIContent **aResult,
                                       already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGViewElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGVIEWELEMENT
  NS_DECL_NSIDOMSVGFITTOVIEWBOX
  NS_DECL_NSIDOMSVGZOOMANDPAN

  // xxx If xpcom allowed virtual inheritance we wouldn't need to
  // forward here :-(
  NS_FORWARD_NSIDOMNODE(nsSVGViewElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGViewElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGViewElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
private:

  // nsSVGElement overrides

  virtual EnumAttributesInfo GetEnumInfo();

  enum { ZOOMANDPAN };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sZoomAndPanMap[];
  static EnumInfo sEnumInfo[1];

  virtual nsSVGViewBox *GetViewBox();
  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio();

  nsSVGViewBox                   mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;

  virtual StringListAttributesInfo GetStringListInfo();

  enum { VIEW_TARGET };
  SVGStringList mStringListAttributes[1];
  static StringListInfo sStringListInfo[1];
};

#endif
