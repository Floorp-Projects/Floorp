/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DDLogValue_h_
#define DDLogValue_h_

#include "DDLogObject.h"
#include "MediaResult.h"
#include "mozilla/Variant.h"
#include "nsString.h"

namespace mozilla {

// Placeholder when no value is needed.
struct DDNoValue
{
};

// Value capturing a range (typically an offset and a number of bytes in a
// resource).
struct DDRange
{
  const int64_t mOffset;
  const int64_t mBytes;
  DDRange(int64_t aOffset, int64_t aBytes)
    : mOffset(aOffset)
    , mBytes(aBytes)
  {
  }
};

// Value associated with a log message.
using DDLogValue = Variant<DDNoValue,
                           DDLogObject,
                           const char*, // Assumed to be a literal string.
                           const nsCString,
                           bool,
                           int8_t,
                           uint8_t,
                           int16_t,
                           uint16_t,
                           int32_t,
                           uint32_t,
                           int64_t,
                           uint64_t,
                           double,
                           DDRange,
                           nsresult,
                           MediaResult>;

void
AppendToString(const DDLogValue& aValue, nsCString& aString);

class JSONWriter;
void
ToJSON(const DDLogValue& aValue,
       JSONWriter& aJSONWriter,
       const char* aPropertyName);

} // namespace mozilla

#endif // DDLogValue_h_
