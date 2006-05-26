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

#include "nsSVGGraphicElement.h"
#include "nsIDOMSVGClipPathElement.h"
#include "nsSVGAtoms.h"
#include "nsSVGAnimatedEnumeration.h"
#include "nsSVGEnum.h"

typedef nsSVGGraphicElement nsSVGClipPathElementBase;

class nsSVGClipPathElement : public nsSVGClipPathElementBase,
                             public nsIDOMSVGClipPathElement
{
protected:
  friend nsresult NS_NewSVGClipPathElement(nsIContent **aResult,
                                           nsINodeInfo *aNodeInfo);
  nsSVGClipPathElement(nsINodeInfo *aNodeInfo);
  nsresult Init();

public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGCLIPPATHELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGClipPathElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGClipPathElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGClipPathElementBase::)

protected:

  // nsIDOMSVGClipPathElement values
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mClipPathUnits;

};

////////////////////////////////////////////////////////////////////////
// implementation


NS_IMPL_NS_NEW_SVG_ELEMENT(ClipPath)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGClipPathElement,nsSVGClipPathElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGClipPathElement,nsSVGClipPathElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGClipPathElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGClipPathElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGClipPathElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGClipPathElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGClipPathElement::nsSVGClipPathElement(nsINodeInfo *aNodeInfo)
  : nsSVGClipPathElementBase(aNodeInfo)
{
}


nsresult
nsSVGClipPathElement::Init()
{
  nsresult rv = nsSVGClipPathElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Define enumeration mappings
  static struct nsSVGEnumMapping gUnitMap[] = {
    {&nsSVGAtoms::objectBoundingBox, nsIDOMSVGClipPathElement::SVG_CPUNITS_OBJECTBOUNDINGBOX},
    {&nsSVGAtoms::userSpaceOnUse, nsIDOMSVGClipPathElement::SVG_CPUNITS_USERSPACEONUSE},
    {nsnull, 0}
  };

  // DOM property: clipPathUnits ,  #IMPLIED attrib: clipPathUnits
  {
    nsCOMPtr<nsISVGEnum> units;
    rv = NS_NewSVGEnum(getter_AddRefs(units),
                       nsIDOMSVGClipPathElement::SVG_CPUNITS_USERSPACEONUSE,
                       gUnitMap);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mClipPathUnits), units);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::clipPathUnits, mClipPathUnits);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration clipPathUnits; */
NS_IMETHODIMP nsSVGClipPathElement::GetClipPathUnits(nsIDOMSVGAnimatedEnumeration * *aClipPathUnits)
{
  *aClipPathUnits = mClipPathUnits;
  NS_IF_ADDREF(*aClipPathUnits);
  return NS_OK;
}


//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGClipPathElement)

