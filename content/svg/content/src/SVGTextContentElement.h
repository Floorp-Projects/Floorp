/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGTextContentElement_h
#define mozilla_dom_SVGTextContentElement_h

#include "mozilla/dom/SVGGraphicsElement.h"
#include "mozilla/dom/SVGAnimatedEnumeration.h"
#include "nsSVGEnum.h"
#include "nsSVGLength2.h"

static const unsigned short SVG_LENGTHADJUST_UNKNOWN          = 0;
static const unsigned short SVG_LENGTHADJUST_SPACING          = 1;
static const unsigned short SVG_LENGTHADJUST_SPACINGANDGLYPHS = 2;

class nsSVGTextFrame2;

namespace mozilla {
class nsISVGPoint;

namespace dom {

class SVGIRect;

typedef SVGGraphicsElement SVGTextContentElementBase;

class SVGTextContentElement : public SVGTextContentElementBase
{
public:
  using FragmentOrElement::TextLength;

  // WebIDL
  already_AddRefed<SVGAnimatedLength> TextLength();
  already_AddRefed<SVGAnimatedEnumeration> LengthAdjust();
  int32_t GetNumberOfChars();
  float GetComputedTextLength();
  void SelectSubString(uint32_t charnum, uint32_t nchars, ErrorResult& rv);
  float GetSubStringLength(uint32_t charnum, uint32_t nchars, ErrorResult& rv);
  already_AddRefed<nsISVGPoint> GetStartPositionOfChar(uint32_t charnum, ErrorResult& rv);
  already_AddRefed<nsISVGPoint> GetEndPositionOfChar(uint32_t charnum, ErrorResult& rv);
  already_AddRefed<SVGIRect> GetExtentOfChar(uint32_t charnum, ErrorResult& rv);
  float GetRotationOfChar(uint32_t charnum, ErrorResult& rv);
  int32_t GetCharNumAtPosition(nsISVGPoint& point);

protected:

  SVGTextContentElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGTextContentElementBase(aNodeInfo)
  {}

  nsSVGTextFrame2* GetSVGTextFrame();

  enum { LENGTHADJUST };
  virtual nsSVGEnum* EnumAttributes() = 0;
  static nsSVGEnumMapping sLengthAdjustMap[];
  static EnumInfo sEnumInfo[1];

  enum { TEXTLENGTH };
  virtual nsSVGLength2* LengthAttributes() = 0;
  static LengthInfo sLengthInfo[1];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGTextContentElement_h
