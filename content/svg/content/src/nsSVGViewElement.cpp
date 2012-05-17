/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is Robert Longson.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  return NS_ERROR_DOM_SVG_INVALID_VALUE_ERR;
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
