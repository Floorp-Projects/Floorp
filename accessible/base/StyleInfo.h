/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set expandtab shiftwidth=2 tabstop=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _mozilla_a11y_style_h_
#define _mozilla_a11y_style_h_

#include "mozilla/gfx/Types.h"
#include "mozilla/AlreadyAddRefed.h"
#include "nsStringFwd.h"
#include "nsColor.h"

class nsAtom;

namespace mozilla {

enum class StyleTextDecorationStyle : uint8_t;

namespace dom {
class Element;
}  // namespace dom
namespace a11y {

class StyleInfo {
 public:
  static void FormatColor(const nscolor& aValue, nsAString& aFormattedValue);
  static already_AddRefed<nsAtom> TextDecorationStyleToAtom(
      StyleTextDecorationStyle aValue);
};

}  // namespace a11y
}  // namespace mozilla

#endif
