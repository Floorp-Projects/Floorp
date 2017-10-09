/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DDMediaLog_h_
#define DDMediaLog_h_

#include "DDLogMessage.h"
#include "nsTArray.h"

namespace mozilla {

namespace dom {
class HTMLMediaElement;
} // namespace dom

class DDLifetimes;

// Container of processed messages corresponding to an HTMLMediaElement (or
// not yet).
struct DDMediaLog
{
  // Associated HTMLMediaElement, or nullptr for the DDMediaLog containing
  // messages for yet-unassociated objects.
  // TODO: Should use a DDLogObject instead, to distinguish between elements
  //       at the same address.
  //       Not critical: At worst we will combine logs for two elements.
  const dom::HTMLMediaElement* mMediaElement;

  // Number of lifetimes associated with this log. Managed by DDMediaLogs.
  int32_t mLifetimeCount = 0;

  using LogMessages = nsTArray<DDLogMessage>;
  LogMessages mMessages;

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;
};

} // namespace mozilla

#endif // DDMediaLog_h_
