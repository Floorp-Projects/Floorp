/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_FlushType_h
#define mozilla_FlushType_h

#include <stdint.h>
#include "X11UndefineNone.h"

namespace mozilla {

/**
 * This is the enum used by nsIDocument::FlushPendingNotifications to
 * decide what to flush.
 *
 * Please note that if you change these values, you should sync it with the
 * flushTypeNames array inside PresShell::FlushPendingNotifications.
 */
enum class FlushType : uint8_t {
  None             = 0, /* Actually don't flush anything */
  Content          = 1, /* flush the content model construction */
  ContentAndNotify = 2, /* As above, plus flush the frame model
                           construction and other nsIMutationObserver
                           notifications. */
  Style            = 3, /* As above, plus flush style reresolution */
  Frames           = Style,
  EnsurePresShellInitAndFrames = 4, /* As above, plus ensure the pres shell is alive */
  InterruptibleLayout = 5, /* As above, plus flush reflow,
                              but allow it to be interrupted (so
                              an incomplete layout may result) */
  Layout           = 6, /* As above, but layout must run to
                           completion */
  Display          = 7, /* As above, plus flush painting */

  Count
};

struct ChangesToFlush {
  ChangesToFlush(FlushType aFlushType, bool aFlushAnimations)
    : mFlushType(aFlushType)
    , mFlushAnimations(aFlushAnimations)
  {}

  FlushType mFlushType;
  bool mFlushAnimations;
};

} // namespace mozilla

#endif // mozilla_FlushType_h
