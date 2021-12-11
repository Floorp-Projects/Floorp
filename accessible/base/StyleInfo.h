/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set expandtab shiftwidth=2 tabstop=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _mozilla_a11y_style_h_
#define _mozilla_a11y_style_h_

#include "mozilla/gfx/Types.h"
#include "mozilla/ComputedStyle.h"

namespace mozilla {
namespace dom {
class Element;
}  // namespace dom
namespace a11y {

class StyleInfo {
 public:
  explicit StyleInfo(dom::Element* aElement);
  ~StyleInfo() {}

  already_AddRefed<nsAtom> Display();
  already_AddRefed<nsAtom> TextAlign();
  mozilla::LengthPercentage TextIndent();
  CSSCoord MarginLeft() { return Margin(eSideLeft); }
  CSSCoord MarginRight() { return Margin(eSideRight); }
  CSSCoord MarginTop() { return Margin(eSideTop); }
  CSSCoord MarginBottom() { return Margin(eSideBottom); }

  static void FormatColor(const nscolor& aValue, nsAString& aFormattedValue);
  static already_AddRefed<nsAtom> TextDecorationStyleToAtom(uint8_t aValue);

 private:
  StyleInfo() = delete;
  StyleInfo(const StyleInfo&) = delete;
  StyleInfo& operator=(const StyleInfo&) = delete;

  CSSCoord Margin(Side aSide);

  dom::Element* mElement;
  RefPtr<ComputedStyle> mComputedStyle;
};

}  // namespace a11y
}  // namespace mozilla

#endif
