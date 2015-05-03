/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGDATAPARSER_H__
#define __NS_SVGDATAPARSER_H__

#include "mozilla/RangedPtr.h"
#include "nsString.h"

////////////////////////////////////////////////////////////////////////
// nsSVGDataParser: a simple base class for parsing values
// for path and transform values.
// 
class nsSVGDataParser
{
public:
  explicit nsSVGDataParser(const nsAString& aValue);

protected:
  static bool IsAlpha(char16_t aCh) {
    // Exclude non-ascii characters before calling isalpha
    return (aCh & 0x7f) == aCh && isalpha(aCh);
  }

  // Returns true if there are more characters to read, false otherwise.
  bool SkipCommaWsp();

  // Returns true if there are more characters to read, false otherwise.
  bool SkipWsp();

  mozilla::RangedPtr<const char16_t> mIter;
  const mozilla::RangedPtr<const char16_t> mEnd;
};


#endif // __NS_SVGDATAPARSER_H__
