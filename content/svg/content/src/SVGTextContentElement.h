/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGTextContentElement_h
#define mozilla_dom_SVGTextContentElement_h

#include "nsIDOMSVGTextContentElement.h"
#include "mozilla/dom/SVGGraphicsElement.h"

class nsSVGTextContainerFrame;

namespace mozilla {
class nsISVGPoint;

namespace dom {

typedef SVGGraphicsElement SVGTextContentElementBase;

class SVGTextContentElement : public SVGTextContentElementBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGTEXTCONTENTELEMENT

  using FragmentOrElement::TextLength;

  // WebIDL
  nsCOMPtr<nsIDOMSVGAnimatedLength> GetTextLength(ErrorResult& rv);
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> GetLengthAdjust(ErrorResult& rv);
  int32_t GetNumberOfChars();
  float GetComputedTextLength();
  float GetSubStringLength(uint32_t charnum, uint32_t nchars, ErrorResult& rv);
  already_AddRefed<nsISVGPoint> GetStartPositionOfChar(uint32_t charnum, ErrorResult& rv);
  already_AddRefed<nsISVGPoint> GetEndPositionOfChar(uint32_t charnum, ErrorResult& rv);
  already_AddRefed<nsIDOMSVGRect> GetExtentOfChar(uint32_t charnum, ErrorResult& rv);
  float GetRotationOfChar(uint32_t charnum, ErrorResult& rv);
  int32_t GetCharNumAtPosition(nsISVGPoint& point);
  void SelectSubString(uint32_t charnum, uint32_t nchars, ErrorResult& rv);

protected:

  SVGTextContentElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGTextContentElementBase(aNodeInfo)
  {}

  nsSVGTextContainerFrame* GetTextContainerFrame();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGTextContentElement_h
