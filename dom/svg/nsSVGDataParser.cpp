/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGDataParser.h"
#include "SVGContentUtils.h"

nsSVGDataParser::nsSVGDataParser(const nsAString& aValue)
  : mIter(SVGContentUtils::GetStartRangedPtr(aValue)),
    mEnd(SVGContentUtils::GetEndRangedPtr(aValue))
{
}

bool
nsSVGDataParser::SkipCommaWsp()
{
  if (!SkipWsp()) {
    // end of string
    return false;
  }
  if (*mIter != ',') {
    return true;
  }
  ++mIter;
  return SkipWsp();
}

bool
nsSVGDataParser::SkipWsp()
{
  while (mIter != mEnd) {
    if (!IsSVGWhitespace(*mIter)) {
      return true;
    }
    ++mIter;
  }
  return false;
}
