/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_ScrollSnap_h_
#define mozilla_layout_ScrollSnap_h_

#include <memory>
#include "mozilla/ScrollTypes.h"
#include "mozilla/ScrollSnapTargetId.h"
#include "mozilla/Maybe.h"

class nsIContent;
class nsIFrame;
struct nsPoint;
struct nsRect;

namespace mozilla {

namespace layers {
struct ScrollSnapInfo;
}

struct ScrollSnapUtils {
  /**
   * GetSnapPointForDestination determines which point to snap to after
   * scrolling. |aStartPos| gives the position before scrolling and
   * |aDestination| gives the position after scrolling, with no snapping.
   * Behaviour is dependent on the value of |aUnit|.
   * |aSnapInfo| and |aScrollRange| are characteristics of the scroll frame for
   * which snapping is being performed.
   * If a suitable snap point could be found, it is returned. Otherwise, an
   * empty Maybe is returned.
   * IMPORTANT NOTE: This function is designed to be called both on and off
   *                 the main thread. If modifying its implementation, be sure
   *                 not to touch main-thread-only data structures without
   *                 appropriate locking.
   */
  static Maybe<SnapTarget> GetSnapPointForDestination(
      const layers::ScrollSnapInfo& aSnapInfo, ScrollUnit aUnit,
      ScrollSnapFlags aSnapFlags, const nsRect& aScrollRange,
      const nsPoint& aStartPos, const nsPoint& aDestination);

  /**
   * Similar to above GetSnapPointForDestination but for re-snapping.
   *
   * |aCurrentPosition| are the snap point(s) last time when we scrolled.
   * |aLastSnapTargetIds| are the snap point(s) last time when we scrolled if
   * exists.
   * |aFocusedContent| is the focused content in the document if exists.
   * Other parameters are same as GetSnapPointForDestination.
   */

  static mozilla::Maybe<mozilla::SnapTarget> GetSnapPointForResnap(
      const layers::ScrollSnapInfo& aSnapInfo, const nsRect& aScrollRange,
      const nsPoint& aCurrentPosition,
      const UniquePtr<ScrollSnapTargetIds>& aLastSnapTargetIds,
      const nsIContent* aFocusedContent);

  static ScrollSnapTargetId GetTargetIdFor(const nsIFrame* aFrame);
};

}  // namespace mozilla

#endif  // mozilla_layout_ScrollSnap_h_
