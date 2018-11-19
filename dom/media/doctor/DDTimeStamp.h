/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DDTimeStamp_h_
#define DDTimeStamp_h_

#include "mozilla/TimeStamp.h"

namespace mozilla {

// Simply using mozilla::TimeStamp as our timestamp type.
using DDTimeStamp = TimeStamp;

inline DDTimeStamp DDNow() { return TimeStamp::Now(); }

// Convert a timestamp to the number of seconds since the process start.
double ToSeconds(const DDTimeStamp& aTimeStamp);

}  // namespace mozilla

#endif  // DDTimeStamp_h_
