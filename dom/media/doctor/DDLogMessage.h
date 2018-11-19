/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DDLogMessage_h_
#define DDLogMessage_h_

#include "DDLogCategory.h"
#include "DDLogObject.h"
#include "DDLogValue.h"
#include "DDMessageIndex.h"
#include "DDTimeStamp.h"
#include "mozilla/Atomics.h"
#include "nsString.h"

namespace mozilla {

class DDLifetimes;

// Structure containing all the information needed in each log message
// (before and after processing).
struct DDLogMessage {
  DDMessageIndex mIndex;
  DDTimeStamp mTimeStamp;
  DDLogObject mObject;
  DDLogCategory mCategory;
  const char* mLabel;
  DDLogValue mValue = DDLogValue{DDNoValue{}};

  // Print the message. Format:
  // "index | timestamp | object | category | label | value". E.g.:
  // "29 | 5.047547 | dom::HTMLMediaElement[134073800] | lnk | decoder |
  // MediaDecoder[136078200]"
  nsCString Print() const;

  // Print the message, using object information from aLifetimes. Format:
  // "index | timestamp | object | category | label | value". E.g.:
  // "29 | 5.047547 | dom::HTMLVideoElement[134073800]#1 (as
  // dom::HTMLMediaElement) | lnk | decoder | MediaSourceDecoder[136078200]#5
  // (as MediaDecoder)"
  nsCString Print(const DDLifetimes& aLifetimes) const;
};

}  // namespace mozilla

#endif  // DDLogMessage_h_
