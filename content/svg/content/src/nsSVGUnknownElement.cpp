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
 * The Initial Developer of the Original Code is Brian O'Keefe.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian O'Keefe <bokeefe@alum.wpi.edu>
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

#include "nsSVGElement.h"
#include "nsIDOMSVGElement.h"

using namespace mozilla;

typedef nsSVGElement nsSVGUnknownElementBase;

class nsSVGUnknownElement : public nsSVGUnknownElementBase,
                            public nsIDOMSVGElement
{
protected:
  friend nsresult NS_NewSVGUnknownElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGUnknownElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  
  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGUnknownElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGUnknownElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGUnknownElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Unknown)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGUnknownElement, nsSVGUnknownElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGUnknownElement, nsSVGUnknownElementBase)

DOMCI_NODE_DATA(SVGUnknownElement, nsSVGUnknownElement)

NS_INTERFACE_TABLE_HEAD(nsSVGUnknownElement)
  NS_NODE_INTERFACE_TABLE3(nsSVGUnknownElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGUnknownElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGUnknownElementBase)
//----------------------------------------------------------------------
// Implementation

nsSVGUnknownElement::nsSVGUnknownElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGUnknownElementBase(aNodeInfo)
{
}

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGUnknownElement)
