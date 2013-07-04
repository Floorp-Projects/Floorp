/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/EncodingUtils.h"

#include "mozilla/Util.h" // ArrayLength
#include "nsUConvPropertySearch.h"

namespace mozilla {
namespace dom {

static const char* labelsEncodings[][3] = {
#include "labelsencodings.properties.h"
};

bool
EncodingUtils::FindEncodingForLabel(const nsACString& aLabel,
                                    nsACString& aOutEncoding)
{
  // Save aLabel first because it may refer the same string as aOutEncoding.
  nsCString label(aLabel);

  EncodingUtils::TrimSpaceCharacters(label);
  if (label.IsEmpty()) {
    aOutEncoding.Truncate();
    return false;
  }

  ToLowerCase(label);
  return NS_SUCCEEDED(nsUConvPropertySearch::SearchPropertyValue(
      labelsEncodings, ArrayLength(labelsEncodings), label, aOutEncoding));
}

bool
EncodingUtils::IsAsciiCompatible(const nsACString& aPreferredName)
{
  return !(aPreferredName.LowerCaseEqualsLiteral("utf-16") ||
           aPreferredName.LowerCaseEqualsLiteral("utf-16be") ||
           aPreferredName.LowerCaseEqualsLiteral("utf-16le") ||
           aPreferredName.LowerCaseEqualsLiteral("utf-7") ||
           aPreferredName.LowerCaseEqualsLiteral("x-imap4-modified-utf7"));
}

} // namespace dom
} // namespace mozilla
