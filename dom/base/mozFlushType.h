/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozFlushType_h___
#define mozFlushType_h___

/**
 * This is the enum used by nsIDocument::FlushPendingNotifications to
 * decide what to flush.
 *
 * Please note that if you change these values, you should sync it with the
 * flushTypeNames array inside PresShell::FlushPendingNotifications.
 */
enum mozFlushType {
  Flush_None             = 0, /* Actually don't flush anything */
  Flush_Content          = 1, /* flush the content model construction */
  Flush_ContentAndNotify = 2, /* As above, plus flush the frame model
                                 construction and other nsIMutationObserver
                                 notifications. */
  Flush_Style            = 3, /* As above, plus flush style reresolution */
  Flush_Frames           = Flush_Style,
  Flush_InterruptibleLayout = 4, /* As above, plus flush reflow,
                                    but allow it to be interrupted (so
                                    an incomplete layout may result) */
  Flush_Layout           = 5, /* As above, but layout must run to
                                 completion */
  Flush_Display          = 6  /* As above, plus flush painting */
};

namespace mozilla {

struct ChangesToFlush {
  ChangesToFlush(mozFlushType aFlushType, bool aFlushAnimations)
    : mFlushType(aFlushType)
    , mFlushAnimations(aFlushAnimations)
  {}

  mozFlushType mFlushType;
  bool mFlushAnimations;
};

}
#endif /* mozFlushType_h___ */
