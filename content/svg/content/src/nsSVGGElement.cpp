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

#include "mozilla/Util.h"

#include "nsSVGGraphicElement.h"
#include "nsIDOMSVGGElement.h"
#include "DOMSVGTests.h"

using namespace mozilla;

typedef nsSVGGraphicElement nsSVGGElementBase;

class nsSVGGElement : public nsSVGGElementBase,
                      public nsIDOMSVGGElement,
                      public DOMSVGTests
{
protected:
  friend nsresult NS_NewSVGGElement(nsIContent **aResult,
                                    already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGGElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGGELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGGElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGGElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGGElementBase::)

  // nsIContent
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

////////////////////////////////////////////////////////////////////////
// implementation


NS_IMPL_NS_NEW_SVG_ELEMENT(G)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGGElement,nsSVGGElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGGElement,nsSVGGElementBase)

DOMCI_NODE_DATA(SVGGElement, nsSVGGElement)

NS_INTERFACE_TABLE_HEAD(nsSVGGElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGGElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTests,
                           nsIDOMSVGGElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGGElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGGElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGGElement::nsSVGGElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGGElementBase(aNodeInfo)
{

}


//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGGElement)


//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGGElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFEFloodMap,
    sFiltersMap,
    sFontSpecificationMap,
    sGradientStopMap,
    sLightingEffectsMap,
    sMarkersMap,
    sTextContentElementsMap,
    sViewportsMap
  };
  
  return FindAttributeDependence(name, map) ||
    nsSVGGElementBase::IsAttributeMapped(name);
}
