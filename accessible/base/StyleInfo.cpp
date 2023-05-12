/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set expandtab shiftwidth=2 tabstop=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StyleInfo.h"

#include "nsStyleConsts.h"

using namespace mozilla;
using namespace mozilla::a11y;

void StyleInfo::FormatColor(const nscolor& aValue, nsAString& aFormattedValue) {
  // Combine the string like rgb(R, G, B) from nscolor.
  // FIXME: What about the alpha channel?
  aFormattedValue.AppendLiteral("rgb(");
  aFormattedValue.AppendInt(NS_GET_R(aValue));
  aFormattedValue.AppendLiteral(", ");
  aFormattedValue.AppendInt(NS_GET_G(aValue));
  aFormattedValue.AppendLiteral(", ");
  aFormattedValue.AppendInt(NS_GET_B(aValue));
  aFormattedValue.Append(')');
}

already_AddRefed<nsAtom> StyleInfo::TextDecorationStyleToAtom(
    StyleTextDecorationStyle aValue) {
  // TODO: When these are enum classes that rust also understands we should just
  // make an FFI call here.
  // TODO: These should probably be static atoms.
  switch (aValue) {
    case StyleTextDecorationStyle::None:
      return NS_Atomize("-moz-none");
    case StyleTextDecorationStyle::Solid:
      return NS_Atomize("solid");
    case StyleTextDecorationStyle::Double:
      return NS_Atomize("double");
    case StyleTextDecorationStyle::Dotted:
      return NS_Atomize("dotted");
    case StyleTextDecorationStyle::Dashed:
      return NS_Atomize("dashed");
    case StyleTextDecorationStyle::Wavy:
      return NS_Atomize("wavy");
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown decoration style");
      break;
  }

  return nullptr;
}
