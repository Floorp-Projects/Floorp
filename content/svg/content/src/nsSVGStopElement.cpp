/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 * Scooter Morris
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Scooter Morris <scootermorris@comcast.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsSVGStylableElement.h"
#include "nsSVGAtoms.h"
#include "nsIDOMSVGStopElement.h"
#include "nsCOMPtr.h"
#include "nsSVGAnimatedNumberList.h"
#include "nsISVGSVGElement.h"
#include "nsSVGNumber.h"
#include "nsSVGAnimatedNumber.h"

typedef nsSVGStylableElement nsSVGStopElementBase;

class nsSVGStopElement : public nsSVGStopElementBase,
                         public nsIDOMSVGStopElement
{
protected:
  friend nsresult NS_NewSVGStopElement(nsIContent **aResult,
                                       nsINodeInfo *aNodeInfo);
  nsSVGStopElement(nsINodeInfo* aNodeInfo);
  virtual ~nsSVGStopElement();
  nsresult Init();
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGSTOPELEMENT

  // xxx If xpcom allowed virtual inheritance we wouldn't need to
  // forward here :-(
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGStopElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGStopElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGStopElementBase::)

  // nsIStyledContent interface
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

protected:

  // nsIDOMSVGStopElement properties:
  nsCOMPtr<nsIDOMSVGAnimatedNumber> mOffset;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Stop)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGStopElement,nsSVGStopElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGStopElement,nsSVGStopElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGStopElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGStopElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGStopElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGStopElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGStopElement::nsSVGStopElement(nsINodeInfo* aNodeInfo)
  : nsSVGStopElementBase(aNodeInfo)
{

}

nsSVGStopElement::~nsSVGStopElement()
{
}

  
nsresult
nsSVGStopElement::Init()
{
  nsresult rv = nsSVGStopElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:

  // DOM property: nsIDOMSVGStopElement::offset, #IMPLIED attrib: offset
  {
    rv = NS_NewSVGAnimatedNumber(getter_AddRefs(mOffset),
                                 0.0);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::offset, mOffset);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGStopElement)

//----------------------------------------------------------------------
// nsIDOMSVGStopElement methods

/* readonly attribute nsIDOMSVGAnimatedLengthList x; */
NS_IMETHODIMP nsSVGStopElement::GetOffset(nsIDOMSVGAnimatedNumber * *aOffset)
{
  *aOffset = mOffset;
  NS_IF_ADDREF(*aOffset);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIStyledContent methods

NS_IMETHODIMP_(PRBool)
nsSVGStopElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sGradientStopMap
  };
  
  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGStopElementBase::IsAttributeMapped(name);
}


