/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/EncodingUtils.h"
#include "nsUConvPropertySearch.h"

namespace mozilla {
namespace dom {

static const char* labelsEncodings[][3] = {
#include "labelsencodings.properties.h"
};

uint32_t
EncodingUtils::IdentifyDataOffset(const char* aData,
                                  const uint32_t aLength,
                                  nsACString& aRetval)
{
  // Truncating to pre-clear return value in case of failure.
  aRetval.Truncate();

  // Minimum bytes in input stream data that represents
  // the Byte Order Mark is 2. Max is 3.
  if (aLength < 2) {
    return 0;
  }

  if (aData[0] == '\xFF' && aData[1] == '\xFE') {
    aRetval.AssignLiteral("UTF-16LE");
    return 2;
  }

  if (aData[0] == '\xFE' && aData[1] == '\xFF') {
    aRetval.AssignLiteral("UTF-16BE");
    return 2;
  }

  // Checking utf-8 byte order mark.
  // Minimum bytes in input stream data that represents
  // the Byte Order Mark for utf-8 is 3.
  if (aLength < 3) {
    return 0;
  }

  if (aData[0] == '\xEF' && aData[1] == '\xBB' && aData[2] == '\xBF') {
    aRetval.AssignLiteral("UTF-8");
    return 3;
  }
  return 0;
}

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

} // namespace dom
} // namespace mozilla
