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
 * The Initial Developer of the Original Code is
 * Scooter Morris.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scooter Morris <scootermorris@comcast.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "mozilla/Util.h"

#include "nsSVGStylableElement.h"
#include "nsIDOMSVGStopElement.h"
#include "nsSVGNumber2.h"
#include "nsSVGUtils.h"
#include "nsGenericHTMLElement.h"

using namespace mozilla;

typedef nsSVGStylableElement nsSVGStopElementBase;

class nsSVGStopElement : public nsSVGStopElementBase,
                         public nsIDOMSVGStopElement
{
protected:
  friend nsresult NS_NewSVGStopElement(nsIContent **aResult,
                                       already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGStopElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGSTOPELEMENT

  // xxx If xpcom allowed virtual inheritance we wouldn't need to
  // forward here :-(
  NS_FORWARD_NSIDOMNODE(nsSVGStopElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGStopElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGStopElementBase::)

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:

  virtual NumberAttributesInfo GetNumberInfo();
  // nsIDOMSVGStopElement properties:
  nsSVGNumber2 mOffset;
  static NumberInfo sNumberInfo;
};

nsSVGElement::NumberInfo nsSVGStopElement::sNumberInfo =
{ &nsGkAtoms::offset, 0, PR_TRUE };

NS_IMPL_NS_NEW_SVG_ELEMENT(Stop)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGStopElement,nsSVGStopElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGStopElement,nsSVGStopElementBase)

DOMCI_NODE_DATA(SVGStopElement, nsSVGStopElement)

NS_INTERFACE_TABLE_HEAD(nsSVGStopElement)
  NS_NODE_INTERFACE_TABLE4(nsSVGStopElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGStopElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGStopElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGStopElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGStopElement::nsSVGStopElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGStopElementBase(aNodeInfo)
{

}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGStopElement)

//----------------------------------------------------------------------
// nsIDOMSVGStopElement methods

/* readonly attribute nsIDOMSVGAnimatedNumber offset; */
NS_IMETHODIMP nsSVGStopElement::GetOffset(nsIDOMSVGAnimatedNumber * *aOffset)
{
  return mOffset.ToDOMAnimatedNumber(aOffset,this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGStopElement::GetNumberInfo()
{
  return NumberAttributesInfo(&mOffset, &sNumberInfo, 1);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGStopElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sGradientStopMap
  };
  
  return FindAttributeDependence(name, map, ArrayLength(map)) ||
    nsSVGStopElementBase::IsAttributeMapped(name);
}


