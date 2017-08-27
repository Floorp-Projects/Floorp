/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DDMessageIndex_h_
#define DDMessageIndex_h_

#include "RollingNumber.h"

namespace mozilla {

// Type used to uniquely identify and sort log messages.
// We assume that a given media element won't live for more than the time taken
// to log 2^31 messages (per process); for 10,000 messages per seconds, that's
// about 2.5 days
using DDMessageIndex = RollingNumber<uint32_t>;

// Printf string constant to use when printing a DDMessageIndex, e.g.:
// `printf("index=%" PRImi, index);`
#define PRImi PRIu32

} // namespace mozilla

#endif // DDMessageIndex_h_
