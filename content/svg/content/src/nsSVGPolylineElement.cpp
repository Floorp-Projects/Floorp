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
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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
#include "nsSVGAtoms.h"
#include "nsSVGPointList.h"
#include "nsIDOMSVGPolylineElement.h"
#include "nsIDOMSVGAnimatedPoints.h"
#include "nsCOMPtr.h"

typedef nsSVGGraphicElement nsSVGPolylineElementBase;

class nsSVGPolylineElement : public nsSVGPolylineElementBase,
                             public nsIDOMSVGPolylineElement,
                             public nsIDOMSVGAnimatedPoints
{
protected:
  friend nsresult NS_NewSVGPolylineElement(nsIContent **aResult,
                                           nsINodeInfo *aNodeInfo);
  nsSVGPolylineElement(nsINodeInfo* aNodeInfo);
  virtual ~nsSVGPolylineElement();
  nsresult Init();
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGPOLYLINEELEMENT
  NS_DECL_NSIDOMSVGANIMATEDPOINTS

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGPolylineElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGPolylineElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGPolylineElementBase::)
  
protected:
  nsCOMPtr<nsIDOMSVGPointList> mPoints;
};


NS_IMPL_NS_NEW_SVG_ELEMENT(Polyline)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGPolylineElement,nsSVGPolylineElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGPolylineElement,nsSVGPolylineElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGPolylineElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPolylineElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedPoints)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGPolylineElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGPolylineElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGPolylineElement::nsSVGPolylineElement(nsINodeInfo* aNodeInfo)
  : nsSVGPolylineElementBase(aNodeInfo)
{

}

nsSVGPolylineElement::~nsSVGPolylineElement()
{
}

  
nsresult
nsSVGPolylineElement::Init()
{
  nsresult rv = nsSVGPolylineElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:
  
  // points #IMPLIED
  rv = nsSVGPointList::Create(getter_AddRefs(mPoints));
  NS_ENSURE_SUCCESS(rv,rv);
  rv = AddMappedSVGValue(nsSVGAtoms::points, mPoints);
  NS_ENSURE_SUCCESS(rv,rv);

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGPolylineElement)


//----------------------------------------------------------------------
// nsIDOMSGAnimatedPoints methods:

/* readonly attribute nsIDOMSVGPointList points; */
NS_IMETHODIMP nsSVGPolylineElement::GetPoints(nsIDOMSVGPointList * *aPoints)
{
  *aPoints = mPoints;
  NS_ADDREF(*aPoints);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGPointList animatedPoints; */
NS_IMETHODIMP nsSVGPolylineElement::GetAnimatedPoints(nsIDOMSVGPointList * *aAnimatedPoints)
{
  *aAnimatedPoints = mPoints;
  NS_ADDREF(*aAnimatedPoints);
  return NS_OK;
}
