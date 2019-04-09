/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGTextContentElement_h
#define mozilla_dom_SVGTextContentElement_h

#include "mozilla/dom/SVGGraphicsElement.h"
#include "DOMSVGAnimatedEnumeration.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedLength.h"

class SVGTextFrame;

namespace mozilla {
class nsISVGPoint;

namespace dom {

class SVGIRect;

typedef SVGGraphicsElement SVGTextContentElementBase;

class SVGTextContentElement : public SVGTextContentElementBase {
  friend class ::SVGTextFrame;

 public:
  using FragmentOrElement::TextLength;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedLength> TextLength();
  already_AddRefed<DOMSVGAnimatedEnumeration> LengthAdjust();
  MOZ_CAN_RUN_SCRIPT int32_t GetNumberOfChars();
  MOZ_CAN_RUN_SCRIPT float GetComputedTextLength();
  MOZ_CAN_RUN_SCRIPT
  void SelectSubString(uint32_t charnum, uint32_t nchars, ErrorResult& rv);
  MOZ_CAN_RUN_SCRIPT
  float GetSubStringLength(uint32_t charnum, uint32_t nchars, ErrorResult& rv);
  MOZ_CAN_RUN_SCRIPT
  already_AddRefed<nsISVGPoint> GetStartPositionOfChar(uint32_t charnum,
                                                       ErrorResult& rv);
  MOZ_CAN_RUN_SCRIPT
  already_AddRefed<nsISVGPoint> GetEndPositionOfChar(uint32_t charnum,
                                                     ErrorResult& rv);
  MOZ_CAN_RUN_SCRIPT
  already_AddRefed<SVGIRect> GetExtentOfChar(uint32_t charnum, ErrorResult& rv);
  MOZ_CAN_RUN_SCRIPT float GetRotationOfChar(uint32_t charnum, ErrorResult& rv);
  MOZ_CAN_RUN_SCRIPT int32_t GetCharNumAtPosition(nsISVGPoint& point);

 protected:
  explicit SVGTextContentElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGTextContentElementBase(std::move(aNodeInfo)) {}

  MOZ_CAN_RUN_SCRIPT SVGTextFrame* GetSVGTextFrame();
  MOZ_CAN_RUN_SCRIPT SVGTextFrame* GetSVGTextFrameForNonLayoutDependentQuery();
  MOZ_CAN_RUN_SCRIPT mozilla::Maybe<int32_t>
  GetNonLayoutDependentNumberOfChars();

  enum { LENGTHADJUST };
  virtual SVGAnimatedEnumeration* EnumAttributes() = 0;
  static SVGEnumMapping sLengthAdjustMap[];
  static EnumInfo sEnumInfo[1];

  enum { TEXTLENGTH };
  virtual SVGAnimatedLength* LengthAttributes() = 0;
  static LengthInfo sLengthInfo[1];
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGTextContentElement_h
