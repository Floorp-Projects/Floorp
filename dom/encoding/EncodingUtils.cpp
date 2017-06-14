/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/EncodingUtils.h"

#include "mozilla/ArrayUtils.h" // ArrayLength
#include "nsUConvPropertySearch.h"

namespace mozilla {
namespace dom {

static constexpr nsUConvProp labelsEncodings[] = {
#include "labelsencodings.properties.h"
};

static constexpr nsUConvProp encodingsGroups[] = {
#include "encodingsgroups.properties.h"
};

bool
EncodingUtils::FindEncodingForLabel(const nsACString& aLabel,
                                    nsACString& aOutEncoding)
{
  auto encoding = Encoding::ForLabel(aLabel);
  if (!encoding) {
    aOutEncoding.Truncate();
    return false;
  }
  encoding->Name(aOutEncoding);
  return true;
}

bool
EncodingUtils::FindEncodingForLabelNoReplacement(const nsACString& aLabel,
                                                 nsACString& aOutEncoding)
{
  auto encoding = Encoding::ForLabelNoReplacement(aLabel);
  if (!encoding) {
    aOutEncoding.Truncate();
    return false;
  }
  encoding->Name(aOutEncoding);
  return true;
}

bool
EncodingUtils::IsAsciiCompatible(const nsACString& aPreferredName)
{
  // HZ and UTF-7 are no longer in mozilla-central, but keeping them here
  // just in case for the benefit of comm-central.
  return !(aPreferredName.LowerCaseEqualsLiteral("utf-16") ||
           aPreferredName.LowerCaseEqualsLiteral("utf-16be") ||
           aPreferredName.LowerCaseEqualsLiteral("utf-16le") ||
           aPreferredName.LowerCaseEqualsLiteral("replacement") ||
           aPreferredName.LowerCaseEqualsLiteral("hz-gb-2312") ||
           aPreferredName.LowerCaseEqualsLiteral("utf-7") ||
           aPreferredName.LowerCaseEqualsLiteral("x-imap4-modified-utf7"));
}

UniquePtr<Decoder>
EncodingUtils::DecoderForEncoding(const nsACString& aEncoding)
{
  return Encoding::ForName(aEncoding)->NewDecoderWithBOMRemoval();
}

UniquePtr<Encoder>
EncodingUtils::EncoderForEncoding(const nsACString& aEncoding)
{
  return Encoding::ForName(aEncoding)->NewEncoder();
}

void
EncodingUtils::LangGroupForEncoding(const nsACString& aEncoding,
                                    nsACString& aOutGroup)
{
  if (NS_FAILED(nsUConvPropertySearch::SearchPropertyValue(
      encodingsGroups, ArrayLength(encodingsGroups), aEncoding, aOutGroup))) {
    aOutGroup.AssignLiteral("x-unicode");
  }
}

} // namespace dom
} // namespace mozilla
