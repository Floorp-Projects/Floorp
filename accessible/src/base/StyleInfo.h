/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _mozilla_a11y_style_h_
#define _mozilla_a11y_style_h_

#include "mozilla/gfx/Types.h"
#include "nsStyleContext.h"

namespace mozilla {
namespace a11y {

class StyleInfo
{
public:
  StyleInfo(dom::Element* aElement, nsIPresShell* aPresShell);
  ~StyleInfo() { };

  void Display(nsAString& aValue);
  void TextAlign(nsAString& aValue);
  void TextIndent(nsAString& aValue);
  void MarginLeft(nsAString& aValue) { Margin(css::eSideLeft, aValue); }
  void MarginRight(nsAString& aValue) { Margin(css::eSideRight, aValue); }
  void MarginTop(nsAString& aValue) { Margin(css::eSideTop, aValue); }
  void MarginBottom(nsAString& aValue) { Margin(css::eSideBottom, aValue); }

  static void FormatColor(const nscolor& aValue, nsString& aFormattedValue);
  static void FormatFontStyle(const nscoord& aValue, nsAString& aFormattedValue);
  static void FormatTextDecorationStyle(PRUint8 aValue, nsAString& aFormattedValue);

private:
  StyleInfo() MOZ_DELETE;
  StyleInfo(const StyleInfo&) MOZ_DELETE;
  StyleInfo& operator = (const StyleInfo&) MOZ_DELETE;

  void Margin(css::Side aSide, nsAString& aValue);

  dom::Element* mElement;
  nsRefPtr<nsStyleContext> mStyleContext;
};

} // namespace a11y
} // namespace mozilla

#endif
