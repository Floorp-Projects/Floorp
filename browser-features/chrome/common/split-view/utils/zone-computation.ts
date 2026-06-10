/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
import type { SplitViewLayout } from "../data/types.js";
export type DropZone = "left" | "right" | "top" | "bottom" | "center";

const DEFAULT_THRESHOLD = 0.33;

/**
 * Determine which edge zone the cursor is in based on normalized coordinates.
 * Each zone covers `threshold` fraction (default 33%) from its respective edge.
 * The center area (not within any edge threshold) returns "center".
 */
export function computeDropZone(
  relX: number,
  relY: number,
  threshold: number = DEFAULT_THRESHOLD,
): DropZone {
  if (relX < threshold) return "left";
  if (relX > 1 - threshold) return "right";
  if (relY < threshold) return "top";
  if (relY > 1 - threshold) return "bottom";
  return "center";
}

/** Map a drop zone to a split view layout. */
export function zoneToLayout(zone: DropZone): "horizontal" | "vertical" {
  if (zone === "left" || zone === "right" || zone === "center")
    return "horizontal";
  return "vertical";
}

/**
 * Determine the tab order for a new split view based on the drop zone.
 * The first tab in the array becomes the left/top pane.
 */
export function zoneToTabOrder<T>(
  zone: DropZone,
  selectedTab: T,
  draggedTab: T,
): [T, T] {
  if (zone === "left" || zone === "top") {
    return [draggedTab, selectedTab];
  }
  return [selectedTab, draggedTab];
}

/**
 * Map a drop zone to a grid-3pane layout when adding a 3rd pane.
 * Returns null for "center" (keep existing layout).
 */
export function zoneToGrid3PaneLayout(zone: DropZone): SplitViewLayout | null {
  switch (zone) {
    case "left":
      return "grid-3pane-left-main";
    case "right":
      return "grid-3pane-right-main";
    case "top":
      return "grid-3pane-top-main";
    case "bottom":
      return "grid-3pane-bottom-main";
    case "center":
      return null;
  }
}

/**
 * Determine whether the newly dropped tab should be moved to the front
 * (column 0 = main pane) for the given grid-3pane layout.
 *
 * In grid-3pane-left-main and grid-3pane-top-main, column 0 is the main
 * (large) pane. In grid-3pane-right-main and grid-3pane-bottom-main,
 * column 2 is the main pane, so the new tab (appended by addTabs) is
 * already in the correct position.
 */
export function shouldMoveNewTabToFront(layout: SplitViewLayout): boolean {
  return layout === "grid-3pane-left-main" || layout === "grid-3pane-top-main";
}

/**
 * Determine the target layout when dropping a tab into an existing split view.
 * Returns the layout to apply, or null to indicate no layout change needed.
 *
 * - 2→3 panes: grid-3pane layout based on zone (center keeps existing)
 * - 3→4 panes: always grid-2x2
 * - Other pane counts: null (just append)
 */
export function computeLayoutForExistingSplit(
  zone: DropZone,
  currentPaneCount: number,
): SplitViewLayout | null {
  const newPaneCount = currentPaneCount + 1;
  if (newPaneCount === 3) {
    return zoneToGrid3PaneLayout(zone);
  }
  if (newPaneCount === 4) {
    return "grid-2x2";
  }
  return null;
}
