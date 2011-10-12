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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsIDOMSVGSymbolElement.h"
#include "nsSVGStylableElement.h"
#include "nsSVGViewBox.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "nsIDOMSVGFitToViewBox.h"
#include "nsGkAtoms.h"

using namespace mozilla;
typedef nsSVGStylableElement nsSVGSymbolElementBase;

class nsSVGSymbolElement : public nsSVGSymbolElementBase,
                           public nsIDOMSVGFitToViewBox,
                           public nsIDOMSVGSymbolElement
{
protected:
  friend nsresult NS_NewSVGSymbolElement(nsIContent **aResult,
                                         already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGSymbolElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGSYMBOLELEMENT
  NS_DECL_NSIDOMSVGFITTOVIEWBOX

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGElement::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGElement::)

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual nsSVGViewBox *GetViewBox();
  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio();

  nsSVGViewBox mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Symbol)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGSymbolElement,nsSVGSymbolElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGSymbolElement,nsSVGSymbolElementBase)

DOMCI_NODE_DATA(SVGSymbolElement, nsSVGSymbolElement)

NS_INTERFACE_TABLE_HEAD(nsSVGSymbolElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGSymbolElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGFitToViewBox,
                           nsIDOMSVGSymbolElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGSymbolElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGSymbolElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGSymbolElement::nsSVGSymbolElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGSymbolElementBase(aNodeInfo)
{
}


//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGSymbolElement)

//----------------------------------------------------------------------
// nsIDOMSVGFitToViewBox methods

/* readonly attribute nsIDOMSVGAnimatedRect viewBox; */
NS_IMETHODIMP nsSVGSymbolElement::GetViewBox(nsIDOMSVGAnimatedRect * *aViewBox)
{
  return mViewBox.ToDOMAnimatedRect(aViewBox, this);
}

/* readonly attribute nsIDOMSVGAnimatedPreserveAspectRatio preserveAspectRatio; */
NS_IMETHODIMP
nsSVGSymbolElement::GetPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio
                                           **aPreserveAspectRatio)
{
  return mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(aPreserveAspectRatio, this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGSymbolElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sFEFloodMap,
    sFillStrokeMap,
    sFiltersMap,
    sFontSpecificationMap,
    sGradientStopMap,
    sGraphicsMap,
    sLightingEffectsMap,
    sMarkersMap,
    sTextContentElementsMap,
    sViewportsMap
   };

  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGSymbolElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGViewBox *
nsSVGSymbolElement::GetViewBox()
{
  return &mViewBox;
}

SVGAnimatedPreserveAspectRatio *
nsSVGSymbolElement::GetPreserveAspectRatio()
{
  return &mPreserveAspectRatio;
}
