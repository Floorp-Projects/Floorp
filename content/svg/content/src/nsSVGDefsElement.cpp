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
#include "nsIDOMSVGDefsElement.h"
#include "nsSVGAtoms.h"

typedef nsSVGGraphicElement nsSVGDefsElementBase;

class nsSVGDefsElement : public nsSVGDefsElementBase,
                         public nsIDOMSVGDefsElement
{
protected:
  friend nsresult NS_NewSVGDefsElement(nsIContent **aResult,
                                    nsINodeInfo *aNodeInfo);
  nsSVGDefsElement(nsINodeInfo *aNodeInfo);
  virtual ~nsSVGDefsElement();
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGDEFSELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGDefsElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGDefsElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGDefsElementBase::)

  // nsIStyledContent
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
protected:
};

////////////////////////////////////////////////////////////////////////
// implementation


NS_IMPL_NS_NEW_SVG_ELEMENT(Defs)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGDefsElement,nsSVGDefsElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGDefsElement,nsSVGDefsElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGDefsElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGDefsElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGDefsElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGDefsElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGDefsElement::nsSVGDefsElement(nsINodeInfo *aNodeInfo)
  : nsSVGDefsElementBase(aNodeInfo)
{

}

nsSVGDefsElement::~nsSVGDefsElement()
{

}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_SVG_DOM_CLONENODE(Defs)


//----------------------------------------------------------------------
// nsIStyledContent methods

NS_IMETHODIMP_(PRBool)
nsSVGDefsElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sTextContentElementsMap,
    sFontSpecificationMap
  };
  
  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGDefsElementBase::IsAttributeMapped(name);
}
