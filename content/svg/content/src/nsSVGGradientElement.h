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

#ifndef __NS_SVGGRADIENTELEMENT_H__
#define __NS_SVGGRADIENTELEMENT_H__

#include "nsIDOMSVGURIReference.h"
#include "nsIDOMSVGGradientElement.h"
#include "nsIDOMSVGUnitTypes.h"
#include "nsSVGStylableElement.h"
#include "nsSVGLength2.h"
#include "nsSVGEnum.h"
#include "nsSVGString.h"
#include "SVGAnimatedTransformList.h"

//--------------------- Gradients------------------------

typedef nsSVGStylableElement nsSVGGradientElementBase;

class nsSVGGradientElement : public nsSVGGradientElementBase,
                             public nsIDOMSVGURIReference,
                             public nsIDOMSVGUnitTypes
{
  friend class nsSVGGradientFrame;

protected:
  nsSVGGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // Gradient Element
  NS_DECL_NSIDOMSVGGRADIENTELEMENT

  // URI Reference
  NS_DECL_NSIDOMSVGURIREFERENCE

  // nsIContent
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual mozilla::SVGAnimatedTransformList* GetAnimatedTransformList();
  virtual nsIAtom* GetTransformListAttrName() const {
    return nsGkAtoms::gradientTransform;
  }

protected:
  virtual EnumAttributesInfo GetEnumInfo();
  virtual StringAttributesInfo GetStringInfo();

  enum { GRADIENTUNITS, SPREADMETHOD };
  nsSVGEnum mEnumAttributes[2];
  static nsSVGEnumMapping sSpreadMethodMap[];
  static EnumInfo sEnumInfo[2];

  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];

  virtual nsresult BeforeSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                 const nsAString* aValue, PRBool aNotify);

  // nsIDOMSVGGradientElement values
  nsAutoPtr<mozilla::SVGAnimatedTransformList> mGradientTransform;
};

//---------------------Linear Gradients------------------------

typedef nsSVGGradientElement nsSVGLinearGradientElementBase;

class nsSVGLinearGradientElement : public nsSVGLinearGradientElementBase,
                                   public nsIDOMSVGLinearGradientElement
{
  friend class nsSVGLinearGradientFrame;

protected:
  friend nsresult NS_NewSVGLinearGradientElement(nsIContent **aResult,
                                                 already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGLinearGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // Gradient Element
  NS_FORWARD_NSIDOMSVGGRADIENTELEMENT(nsSVGLinearGradientElementBase::)

  // Linear Gradient
  NS_DECL_NSIDOMSVGLINEARGRADIENTELEMENT

  // The Gradient Element base class implements these
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGLinearGradientElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGLinearGradientElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGLinearGradientElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:

  virtual LengthAttributesInfo GetLengthInfo();

  // nsIDOMSVGLinearGradientElement values
  enum { X1, Y1, X2, Y2 };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

//-------------------------- Radial Gradients ----------------------------

typedef nsSVGGradientElement nsSVGRadialGradientElementBase;

class nsSVGRadialGradientElement : public nsSVGRadialGradientElementBase,
                                   public nsIDOMSVGRadialGradientElement
{
  friend class nsSVGRadialGradientFrame;

protected:
  friend nsresult NS_NewSVGRadialGradientElement(nsIContent **aResult,
                                                 already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGRadialGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED

  // Gradient Element
  NS_FORWARD_NSIDOMSVGGRADIENTELEMENT(nsSVGRadialGradientElementBase::)

  // Radial Gradient
  NS_DECL_NSIDOMSVGRADIALGRADIENTELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGRadialGradientElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGRadialGradientElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGRadialGradientElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:

  virtual LengthAttributesInfo GetLengthInfo();

  // nsIDOMSVGRadialGradientElement values
  enum { CX, CY, R, FX, FY };
  nsSVGLength2 mLengthAttributes[5];
  static LengthInfo sLengthInfo[5];
};

#endif
