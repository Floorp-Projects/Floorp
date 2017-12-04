/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Include file for fixing symbol visibility on Linux.

#ifndef MOZILLA_AVUTIL_VISIBILITY_H
#define MOZILLA_AVUTIL_VISIBILITY_H

// We need to preemptively include <stdlib.h> before anyone[1] has a chance
// to include <limits.h>. We do this to avoid a linux clang build error, in
// -ffreestanding mode on automation, which happens when limits.h defines
// MB_LEN_MAX to some value that is different from what stdlib.h expects. If
// we include stdlib.h before limits.h, then they don't get a chance to
// interact badly.
//
// [1] (e.g. libavutil/common.h, which is indirectly included by log.h below.)
#include <stdlib.h>

#pragma GCC visibility push(default)
#include "libavutil/cpu.h"

// We need av_log() to be visible so we can enable assertions in libavcodec.
#include "libavutil/log.h"

#pragma GCC visibility pop

#endif // MOZILLA_AVUTIL_VISIBILITY_H
