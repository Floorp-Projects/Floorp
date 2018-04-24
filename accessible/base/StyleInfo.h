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
} // namespace dom
namespace a11y {

class StyleInfo
{
public:
  explicit StyleInfo(dom::Element* aElement);
  ~StyleInfo() { }

  void Display(nsAString& aValue);
  void TextAlign(nsAString& aValue);
  void TextIndent(nsAString& aValue);
  void MarginLeft(nsAString& aValue) { Margin(eSideLeft, aValue); }
  void MarginRight(nsAString& aValue) { Margin(eSideRight, aValue); }
  void MarginTop(nsAString& aValue) { Margin(eSideTop, aValue); }
  void MarginBottom(nsAString& aValue) { Margin(eSideBottom, aValue); }

  static void FormatColor(const nscolor& aValue, nsString& aFormattedValue);
  static void FormatTextDecorationStyle(uint8_t aValue, nsAString& aFormattedValue);

private:
  StyleInfo() = delete;
  StyleInfo(const StyleInfo&) = delete;
  StyleInfo& operator = (const StyleInfo&) = delete;

  void Margin(Side aSide, nsAString& aValue);

  dom::Element* mElement;
  RefPtr<ComputedStyle> mComputedStyle;
};

} // namespace a11y
} // namespace mozilla

#endif
