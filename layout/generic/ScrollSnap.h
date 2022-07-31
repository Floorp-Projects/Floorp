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
struct nsSize;

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

  // Post a pending re-snap request if the given |aFrame| is one of the snap
  // points on the last scroll operation.
  static void PostPendingResnapIfNeededFor(nsIFrame* aFrame);

  // Similar to above PostPendingResnapIfNeededFor but post a pending re-snap
  // request even if the given |aFrame| is not one of the last snap point.
  // This is basically used for cases there was no valid snap point on the last
  // scroll operation but the given |aFrame| might be a valid snap point now,
  // e.g changing the scroll-snap-align property from `none` to something.
  static void PostPendingResnapFor(nsIFrame* aFrame);

  // Returns true if the writing-mode of the snap target element needs to be
  // respected to resolve scroll-snap-align property.
  // Note that usually the scroll container's writing-mode is used for resolving
  // the property but there's a special case defined in the CSS scroll snap
  // spec.
  static bool NeedsToRespectTargetWritingMode(const nsSize& aSnapAreaSize,
                                              const nsSize& aSnapportSize);

  // Returns the scroll snap area for the snap target frame |aFrame| inside the
  // nearest scroll container |aScrolledFrame| and its scrolled rect
  // |aScrolledRect|.
  static nsRect GetSnapAreaFor(const nsIFrame* aFrame,
                               const nsIFrame* aScrolledFrame,
                               const nsRect& aScrolledRect);
};

}  // namespace mozilla

#endif  // mozilla_layout_ScrollSnap_h_
