/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DisplayPortUtils_h__
#define mozilla_DisplayPortUtils_h__

#include "Units.h"
#include "nsDisplayList.h"
#include "nsRect.h"

#include <cstdint>

class nsIContent;
class nsIFrame;
class nsDisplayListBuilder;
class nsPresContext;

namespace mozilla {

class PresShell;

struct DisplayPortPropertyData {
  DisplayPortPropertyData(const nsRect& aRect, uint32_t aPriority,
                          bool aPainted)
      : mRect(aRect), mPriority(aPriority), mPainted(aPainted) {}
  nsRect mRect;
  uint32_t mPriority;
  bool mPainted;
};

struct DisplayPortMarginsPropertyData {
  DisplayPortMarginsPropertyData(const ScreenMargin& aMargins,
                                 uint32_t aPriority, bool aPainted)
      : mMargins(aMargins), mPriority(aPriority), mPainted(aPainted) {}
  ScreenMargin mMargins;
  uint32_t mPriority;
  bool mPainted;
};

// For GetDisplayPort
enum class DisplayportRelativeTo { ScrollPort, ScrollFrame };

class DisplayPortUtils {
 public:
  /**
   * Get display port for the given element, relative to the specified entity,
   * defaulting to the scrollport.
   */
  static bool GetDisplayPort(
      nsIContent* aContent, nsRect* aResult,
      DisplayportRelativeTo aRelativeTo = DisplayportRelativeTo::ScrollPort,
      bool* aOutPainted = nullptr);

  /**
   * Check whether the given element has a displayport.
   */
  static bool HasDisplayPort(nsIContent* aContent);

  /**
   * Check whether the given element has a displayport that has already
   * been sent to the compositor via a layers or WR transaction.
   */
  static bool HasPaintedDisplayPort(nsIContent* aContent);

  /**
   * Mark the displayport of a given element as having been sent to
   * the compositor via a layers or WR transaction.
   */
  static void MarkDisplayPortAsPainted(nsIContent* aContent);

  /**
   * Check whether the given frame has a displayport. It returns false
   * for scrolled frames and true for the corresponding scroll frame.
   * Optionally pass the child, and it only returns true if the child is the
   * scrolled frame for the displayport.
   */
  static bool FrameHasDisplayPort(nsIFrame* aFrame,
                                  const nsIFrame* aScrolledFrame = nullptr);

  /**
   * Check if the given element has a margins based displayport but is missing a
   * displayport base rect that it needs to properly compute a displayport rect.
   */
  static bool IsMissingDisplayPortBaseRect(nsIContent* aContent);

  /**
   * Go through the IPC Channel and update displayport margins for content
   * elements based on UpdateFrame messages. The messages are left in the
   * queue and will be fully processed when dequeued. The aim is to paint
   * the most up-to-date displayport without waiting for these message to
   * go through the message queue.
   */
  static void UpdateDisplayPortMarginsFromPendingMessages();

  /**
   * @return the display port for the given element which should be used for
   * visibility testing purposes.
   *
   * If low-precision buffers are enabled, this is the critical display port;
   * otherwise, it's the same display port returned by GetDisplayPort().
   */
  static bool GetDisplayPortForVisibilityTesting(
      nsIContent* aContent, nsRect* aResult,
      DisplayportRelativeTo aRelativeTo = DisplayportRelativeTo::ScrollPort);

  enum class RepaintMode : uint8_t { Repaint, DoNotRepaint };

  /**
   * Invalidate for displayport change.
   */
  static void InvalidateForDisplayPortChange(
      nsIContent* aContent, bool aHadDisplayPort, const nsRect& aOldDisplayPort,
      const nsRect& aNewDisplayPort,
      RepaintMode aRepaintMode = RepaintMode::Repaint);

  /**
   * Set the display port margins for a content element to be used with a
   * display port base (see SetDisplayPortBase()).
   * See also nsIDOMWindowUtils.setDisplayPortMargins.
   * @param aContent the content element for which to set the margins
   * @param aPresShell the pres shell for the document containing the element
   * @param aMargins the margins to set
   * @param aAlignmentX, alignmentY the amount of pixels to which to align the
   *                                displayport built by combining the base
   *                                rect with the margins, in either direction
   * @param aPriority a priority value to determine which margins take effect
   *                  when multiple callers specify margins
   * @param aRepaintMode whether to schedule a paint after setting the margins
   * @return true if the new margins were applied.
   */
  static bool SetDisplayPortMargins(
      nsIContent* aContent, PresShell* aPresShell, const ScreenMargin& aMargins,
      uint32_t aPriority = 0, RepaintMode aRepaintMode = RepaintMode::Repaint);

  /**
   * Set the display port base rect for given element to be used with display
   * port margins.
   * SetDisplayPortBaseIfNotSet is like SetDisplayPortBase except it only sets
   * the display port base to aBase if no display port base is currently set.
   */
  static void SetDisplayPortBase(nsIContent* aContent, const nsRect& aBase);
  static void SetDisplayPortBaseIfNotSet(nsIContent* aContent,
                                         const nsRect& aBase);

  /**
   * Get the critical display port for the given element.
   */
  static bool GetCriticalDisplayPort(nsIContent* aContent, nsRect* aResult,
                                     bool* aOutPainted = nullptr);

  /**
   * Check whether the given element has a critical display port.
   */
  static bool HasCriticalDisplayPort(nsIContent* aContent);

  /**
   * If low-precision painting is turned on, delegates to
   * GetCriticalDisplayPort. Otherwise, delegates to GetDisplayPort.
   */
  static bool GetHighResolutionDisplayPort(nsIContent* aContent,
                                           nsRect* aResult,
                                           bool* aOutPainted = nullptr);

  /**
   * Remove the displayport for the given element.
   */
  static void RemoveDisplayPort(nsIContent* aContent);

  /**
   * Return true if aPresContext's viewport has a displayport.
   */
  static bool ViewportHasDisplayPort(nsPresContext* aPresContext);

  /**
   * Return true if aFrame is a fixed-pos frame and is a child of a viewport
   * which has a displayport. These frames get special treatment from the
   * compositor. aDisplayPort, if non-null, is set to the display port rectangle
   * (relative to the viewport).
   */
  static bool IsFixedPosFrameInDisplayPort(const nsIFrame* aFrame);

  static bool MaybeCreateDisplayPortInFirstScrollFrameEncountered(
      nsIFrame* aFrame, nsDisplayListBuilder* aBuilder);

  /**
   * Calculate a default set of displayport margins for the given scrollframe
   * and set them on the scrollframe's content element. The margins are set with
   * the default priority, which may clobber previously set margins. The repaint
   * mode provided is passed through to the call to SetDisplayPortMargins.
   * The |aScrollFrame| parameter must be non-null and queryable to an nsIFrame.
   * @return true iff the call to SetDisplayPortMargins returned true.
   */
  static bool CalculateAndSetDisplayPortMargins(
      nsIScrollableFrame* aScrollFrame, RepaintMode aRepaintMode);

  /**
   * If |aScrollFrame| WantsAsyncScroll() and we don't have a scrollable
   * displayport yet (as tracked by |aBuilder|), calculate and set a
   * displayport.
   *
   * If this is called during display list building pass DoNotRepaint in
   * aRepaintMode.
   *
   * Returns true if there is a displayport on an async scrollable scrollframe
   * after this call, either because one was just added or it already existed.
   */
  static bool MaybeCreateDisplayPort(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aScrollFrame,
                                     RepaintMode aRepaintMode);

  /**
   * Sets a zero margin display port on all proper ancestors of aFrame that
   * are async scrollable.
   */
  static void SetZeroMarginDisplayPortOnAsyncScrollableAncestors(
      nsIFrame* aFrame);

  /**
   * Finds the closest ancestor async scrollable frame from aFrame that has a
   * displayport and attempts to trigger the displayport expiry on that
   * ancestor.
   */
  static void ExpireDisplayPortOnAsyncScrollableAncestor(nsIFrame* aFrame);
};

}  // namespace mozilla

#endif  // mozilla_DisplayPortUtils_h__
