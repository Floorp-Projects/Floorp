/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGTextContentElement_h
#define mozilla_dom_SVGTextContentElement_h

#include "mozilla/dom/SVGGraphicsElement.h"

class nsSVGTextContainerFrame;
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
  int32_t GetNumberOfChars();
  float GetComputedTextLength();
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

  nsSVGTextContainerFrame* GetTextContainerFrame();
  nsSVGTextFrame2* GetSVGTextFrame();
  bool FrameIsSVGText();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGTextContentElement_h
