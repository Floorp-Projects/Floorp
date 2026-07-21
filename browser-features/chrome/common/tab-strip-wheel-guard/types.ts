/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export type StreamAxis = "h" | "v";

/** Minimal shape of #tabbrowser-arrowscrollbox the guard reads/wraps. */
export type GuardedScrollbox = XULElement & {
  isRTLScrollbox?: boolean;
  ensureElementIsVisible: (el: Element, instant?: boolean) => void;
  __floorpRecentreWrapped?: boolean;
};

/** Minimal shape of the gBrowser global the guard reads. */
export interface TabBrowserLike {
  selectedTab: Element;
}

/** One consecutive run of dropped events (mutated in place while running). */
export interface DroppedRun {
  axis: StreamAxis;
  events: number;
  px: number;
  endedAt: number;
}

/** Readout published at globalThis.__floorpWheelGuard — the elimination
 * readout: after a reproduction attempt, non-zero counters mean the guard
 * engaged on that gesture. */
export interface WheelGuardReadout {
  axisDropped: number;
  reversalDropped: number;
  recentresSuppressed: number;
  lastRun: DroppedRun | null;
  reset: () => void;
}
