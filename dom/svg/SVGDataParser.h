/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGDATAPARSER_H_
#define DOM_SVG_SVGDATAPARSER_H_

#include "mozilla/RangedPtr.h"
#include "nsStringFwd.h"

namespace mozilla {

////////////////////////////////////////////////////////////////////////
// SVGDataParser: a simple base class for parsing values
// for path and transform values.
//
class SVGDataParser {
 public:
  explicit SVGDataParser(const nsAString& aValue);

 protected:
  // Returns true if there are more characters to read, false otherwise.
  bool SkipCommaWsp();

  // Returns true if there are more characters to read, false otherwise.
  bool SkipWsp();

  mozilla::RangedPtr<const char16_t> mIter;
  const mozilla::RangedPtr<const char16_t> mEnd;
};

}  // namespace mozilla

#endif  // DOM_SVG_SVGDATAPARSER_H_
