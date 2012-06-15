/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGViewElement.h"
#include "DOMSVGStringList.h"

using namespace mozilla;

nsSVGElement::StringListInfo nsSVGViewElement::sStringListInfo[1] =
{
  { &nsGkAtoms::viewTarget }
};

nsSVGEnumMapping nsSVGViewElement::sZoomAndPanMap[] = {
  {&nsGkAtoms::disable, nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_DISABLE},
  {&nsGkAtoms::magnify, nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY},
  {nsnull, 0}
};

nsSVGElement::EnumInfo nsSVGViewElement::sEnumInfo[1] =
{
  { &nsGkAtoms::zoomAndPan,
    sZoomAndPanMap,
    nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY
  }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(View)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGViewElement,nsSVGViewElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGViewElement,nsSVGViewElementBase)

DOMCI_NODE_DATA(SVGViewElement, nsSVGViewElement)

NS_INTERFACE_TABLE_HEAD(nsSVGViewElement)
  NS_NODE_INTERFACE_TABLE6(nsSVGViewElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGViewElement,
                           nsIDOMSVGFitToViewBox,
                           nsIDOMSVGZoomAndPan)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGViewElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGViewElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGViewElement::nsSVGViewElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGViewElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGViewElement)

//----------------------------------------------------------------------
// nsIDOMSVGZoomAndPan methods

/* attribute unsigned short zoomAndPan; */
NS_IMETHODIMP
nsSVGViewElement::GetZoomAndPan(PRUint16 *aZoomAndPan)
{
  *aZoomAndPan = mEnumAttributes[ZOOMANDPAN].GetAnimValue();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGViewElement::SetZoomAndPan(PRUint16 aZoomAndPan)
{
  if (aZoomAndPan == nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_DISABLE ||
      aZoomAndPan == nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY) {
    mEnumAttributes[ZOOMANDPAN].SetBaseValue(aZoomAndPan, this);
    return NS_OK;
  }

  return NS_ERROR_RANGE_ERR;
}

//----------------------------------------------------------------------
// nsIDOMSVGFitToViewBox methods

/* readonly attribute nsIDOMSVGAnimatedRect viewBox; */
NS_IMETHODIMP
nsSVGViewElement::GetViewBox(nsIDOMSVGAnimatedRect * *aViewBox)
{
  return mViewBox.ToDOMAnimatedRect(aViewBox, this);
}

/* readonly attribute nsIDOMSVGAnimatedPreserveAspectRatio preserveAspectRatio; */
NS_IMETHODIMP
nsSVGViewElement::GetPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio
                                         **aPreserveAspectRatio)
{
  return mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(aPreserveAspectRatio, this);
}

//----------------------------------------------------------------------
// nsIDOMSVGViewElement methods

/* readonly attribute nsIDOMSVGStringList viewTarget; */
NS_IMETHODIMP nsSVGViewElement::GetViewTarget(nsIDOMSVGStringList * *aViewTarget)
{
  *aViewTarget = DOMSVGStringList::GetDOMWrapper(
                   &mStringListAttributes[VIEW_TARGET], this, false, VIEW_TARGET).get();
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::EnumAttributesInfo
nsSVGViewElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGViewBox *
nsSVGViewElement::GetViewBox()
{
  return &mViewBox;
}

SVGAnimatedPreserveAspectRatio *
nsSVGViewElement::GetPreserveAspectRatio()
{
  return &mPreserveAspectRatio;
}

nsSVGElement::StringListAttributesInfo
nsSVGViewElement::GetStringListInfo()
{
  return StringListAttributesInfo(mStringListAttributes, sStringListInfo,
                                  ArrayLength(sStringListInfo));
}
