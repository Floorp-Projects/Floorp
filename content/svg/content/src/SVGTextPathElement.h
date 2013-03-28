/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGTextPathElement_h
#define mozilla_dom_SVGTextPathElement_h

#include "nsSVGEnum.h"
#include "nsSVGLength2.h"
#include "nsSVGString.h"
#include "mozilla/dom/SVGTextContentElement.h"

class nsIAtom;
class nsIContent;
class nsINodeInfo;
class nsSVGTextPathFrame;

nsresult NS_NewSVGTextPathElement(nsIContent **aResult,
                                  already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

// textPath Method Types
static const unsigned short TEXTPATH_METHODTYPE_UNKNOWN  = 0;
static const unsigned short TEXTPATH_METHODTYPE_ALIGN    = 1;
static const unsigned short TEXTPATH_METHODTYPE_STRETCH  = 2;
// textPath Spacing Types
static const unsigned short TEXTPATH_SPACINGTYPE_UNKNOWN = 0;
static const unsigned short TEXTPATH_SPACINGTYPE_AUTO    = 1;
static const unsigned short TEXTPATH_SPACINGTYPE_EXACT   = 2;

typedef SVGTextContentElement SVGTextPathElementBase;

class SVGTextPathElement MOZ_FINAL : public SVGTextPathElementBase
{
friend class ::nsSVGTextPathFrame;
friend class ::nsSVGTextFrame2;

protected:
  friend nsresult (::NS_NewSVGTextPathElement(nsIContent **aResult,
                                              already_AddRefed<nsINodeInfo> aNodeInfo));
  SVGTextPathElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx, JSObject *scope) MOZ_OVERRIDE;

public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual bool IsEventAttributeName(nsIAtom* aName) MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<SVGAnimatedLength> StartOffset();
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> Method();
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> Spacing();
  already_AddRefed<nsIDOMSVGAnimatedString> Href();

 protected:

  virtual LengthAttributesInfo GetLengthInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  virtual StringAttributesInfo GetStringInfo();

  enum { STARTOFFSET };
  nsSVGLength2 mLengthAttributes[1];
  static LengthInfo sLengthInfo[1];

  enum { METHOD, SPACING };
  nsSVGEnum mEnumAttributes[2];
  static nsSVGEnumMapping sMethodMap[];
  static nsSVGEnumMapping sSpacingMap[];
  static EnumInfo sEnumInfo[2];

  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGTextPathElement_h
