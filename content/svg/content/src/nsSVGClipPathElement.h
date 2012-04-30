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

#ifndef __NS_SVGCLIPPATHELEMENT_H__
#define __NS_SVGCLIPPATHELEMENT_H__

#include "DOMSVGTests.h"
#include "nsIDOMSVGClipPathElement.h"
#include "nsIDOMSVGUnitTypes.h"
#include "nsSVGEnum.h"
#include "nsSVGGraphicElement.h"

typedef nsSVGGraphicElement nsSVGClipPathElementBase;

class nsSVGClipPathElement : public nsSVGClipPathElementBase,
                             public nsIDOMSVGClipPathElement,
                             public DOMSVGTests,
                             public nsIDOMSVGUnitTypes
{
  friend class nsSVGClipPathFrame;

protected:
  friend nsresult NS_NewSVGClipPathElement(nsIContent **aResult,
                                           already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGClipPathElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGCLIPPATHELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGClipPathElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGClipPathElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGClipPathElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:

  // nsIDOMSVGClipPathElement values
  enum { CLIPPATHUNITS };
  nsSVGEnum mEnumAttributes[1];
  static EnumInfo sEnumInfo[1];

  virtual EnumAttributesInfo GetEnumInfo();
};

#endif
