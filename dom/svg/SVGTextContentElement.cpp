/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGTextContentElement.h"

#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/dom/SVGTextContentElementBinding.h"
#include "mozilla/dom/SVGRect.h"
#include "nsBidiUtils.h"
#include "DOMSVGPoint.h"
#include "nsLayoutUtils.h"
#include "nsTextFragment.h"
#include "nsTextFrameUtils.h"
#include "nsTextNode.h"
#include "SVGTextFrame.h"

namespace mozilla::dom {

using namespace SVGTextContentElement_Binding;

SVGEnumMapping SVGTextContentElement::sLengthAdjustMap[] = {
    {nsGkAtoms::spacing, LENGTHADJUST_SPACING},
    {nsGkAtoms::spacingAndGlyphs, LENGTHADJUST_SPACINGANDGLYPHS},
    {nullptr, 0}};

SVGElement::EnumInfo SVGTextContentElement::sEnumInfo[1] = {
    {nsGkAtoms::lengthAdjust, sLengthAdjustMap, LENGTHADJUST_SPACING}};

SVGElement::LengthInfo SVGTextContentElement::sLengthInfo[1] = {
    {nsGkAtoms::textLength, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::XY}};

SVGTextFrame* SVGTextContentElement::GetSVGTextFrame() {
  nsIFrame* frame = GetPrimaryFrame(FlushType::Layout);
  nsIFrame* textFrame =
      nsLayoutUtils::GetClosestFrameOfType(frame, LayoutFrameType::SVGText);
  return static_cast<SVGTextFrame*>(textFrame);
}

SVGTextFrame*
SVGTextContentElement::GetSVGTextFrameForNonLayoutDependentQuery() {
  nsIFrame* frame = GetPrimaryFrame(FlushType::Frames);
  nsIFrame* textFrame =
      nsLayoutUtils::GetClosestFrameOfType(frame, LayoutFrameType::SVGText);
  return static_cast<SVGTextFrame*>(textFrame);
}

already_AddRefed<DOMSVGAnimatedLength> SVGTextContentElement::TextLength() {
  return LengthAttributes()[TEXTLENGTH].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedEnumeration>
SVGTextContentElement::LengthAdjust() {
  return EnumAttributes()[LENGTHADJUST].ToDOMAnimatedEnum(this);
}

//----------------------------------------------------------------------

template <typename T>
static bool FragmentHasSkippableCharacter(const T* aBuffer, uint32_t aLength) {
  for (uint32_t i = 0; i < aLength; i++) {
    if (nsTextFrameUtils::IsSkippableCharacterForTransformText(aBuffer[i])) {
      return true;
    }
  }
  return false;
}

Maybe<int32_t> SVGTextContentElement::GetNonLayoutDependentNumberOfChars() {
  SVGTextFrame* frame = GetSVGTextFrameForNonLayoutDependentQuery();
  if (!frame || frame != GetPrimaryFrame()) {
    // Only support this fast path on <text>, not child <tspan>s, etc.
    return Nothing();
  }

  uint32_t num = 0;

  for (nsINode* n = Element::GetFirstChild(); n; n = n->GetNextSibling()) {
    if (!n->IsText()) {
      return Nothing();
    }

    const nsTextFragment* text = &n->AsText()->TextFragment();
    uint32_t length = text->GetLength();

    if (text->Is2b()) {
      if (FragmentHasSkippableCharacter(text->Get2b(), length)) {
        return Nothing();
      }
    } else {
      auto buffer = reinterpret_cast<const uint8_t*>(text->Get1b());
      if (FragmentHasSkippableCharacter(buffer, length)) {
        return Nothing();
      }
    }

    num += length;
  }

  return Some(num);
}

int32_t SVGTextContentElement::GetNumberOfChars() {
  Maybe<int32_t> num = GetNonLayoutDependentNumberOfChars();
  if (num) {
    return *num;
  }

  SVGTextFrame* textFrame = GetSVGTextFrame();
  return textFrame ? textFrame->GetNumberOfChars(this) : 0;
}

float SVGTextContentElement::GetComputedTextLength() {
  SVGTextFrame* textFrame = GetSVGTextFrame();
  return textFrame ? textFrame->GetComputedTextLength(this) : 0.0f;
}

void SVGTextContentElement::SelectSubString(uint32_t charnum, uint32_t nchars,
                                            ErrorResult& rv) {
  SVGTextFrame* textFrame = GetSVGTextFrame();
  if (!textFrame) return;

  textFrame->SelectSubString(this, charnum, nchars, rv);
}

float SVGTextContentElement::GetSubStringLength(uint32_t charnum,
                                                uint32_t nchars,
                                                ErrorResult& rv) {
  SVGTextFrame* textFrame = GetSVGTextFrameForNonLayoutDependentQuery();
  if (!textFrame) return 0.0f;

  if (!textFrame->RequiresSlowFallbackForSubStringLength()) {
    return textFrame->GetSubStringLengthFastPath(this, charnum, nchars, rv);
  }
  // We need to make sure that we've been reflowed before using the slow
  // fallback path as it may affect glyph positioning. GetSVGTextFrame will do
  // that for us.
  // XXX perf: It may be possible to limit reflow to just calling ReflowSVG,
  // but we would still need to resort to full reflow for percentage
  // positioning attributes.  For now we just do a full reflow regardless
  // since the cases that would cause us to be called are relatively uncommon.
  textFrame = GetSVGTextFrame();
  if (!textFrame) return 0.0f;

  return textFrame->GetSubStringLengthSlowFallback(this, charnum, nchars, rv);
}

already_AddRefed<DOMSVGPoint> SVGTextContentElement::GetStartPositionOfChar(
    uint32_t charnum, ErrorResult& rv) {
  SVGTextFrame* textFrame = GetSVGTextFrame();
  if (!textFrame) {
    rv.ThrowInvalidStateError("No layout information available for SVG text");
    return nullptr;
  }

  return textFrame->GetStartPositionOfChar(this, charnum, rv);
}

already_AddRefed<DOMSVGPoint> SVGTextContentElement::GetEndPositionOfChar(
    uint32_t charnum, ErrorResult& rv) {
  SVGTextFrame* textFrame = GetSVGTextFrame();
  if (!textFrame) {
    rv.ThrowInvalidStateError("No layout information available for SVG text");
    return nullptr;
  }

  return textFrame->GetEndPositionOfChar(this, charnum, rv);
}

already_AddRefed<SVGRect> SVGTextContentElement::GetExtentOfChar(
    uint32_t charnum, ErrorResult& rv) {
  SVGTextFrame* textFrame = GetSVGTextFrame();

  if (!textFrame) {
    rv.ThrowInvalidStateError("No layout information available for SVG text");
    return nullptr;
  }

  return textFrame->GetExtentOfChar(this, charnum, rv);
}

float SVGTextContentElement::GetRotationOfChar(uint32_t charnum,
                                               ErrorResult& rv) {
  SVGTextFrame* textFrame = GetSVGTextFrame();

  if (!textFrame) {
    rv.ThrowInvalidStateError("No layout information available for SVG text");
    return 0.0f;
  }

  return textFrame->GetRotationOfChar(this, charnum, rv);
}

int32_t SVGTextContentElement::GetCharNumAtPosition(
    const DOMPointInit& aPoint) {
  SVGTextFrame* textFrame = GetSVGTextFrame();
  return textFrame ? textFrame->GetCharNumAtPosition(this, aPoint) : -1;
}

}  // namespace mozilla::dom
