/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Include file for fixing symbol visibility on Linux.

#ifndef MOZILLA_AVUTIL_VISIBILITY_H
#define MOZILLA_AVUTIL_VISIBILITY_H

#pragma GCC visibility push(default)
#include "libavutil/cpu.h"

// We need av_log() to be visible so we can enable assertions in libavcodec.
#include "libavutil/log.h"

#pragma GCC visibility pop

#endif // MOZILLA_AVUTIL_VISIBILITY_H
