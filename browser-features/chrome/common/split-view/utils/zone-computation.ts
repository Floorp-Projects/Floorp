/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export type DropZone = "left" | "right" | "top" | "bottom";

const DEFAULT_THRESHOLD = 0.25;
const DEFAULT_ZONE: DropZone = "right";

/**
 * Determine which edge zone the cursor is in based on normalized coordinates.
 * Each zone covers `threshold` fraction (default 25%) from its respective edge.
 */
export function computeDropZone(
  relX: number,
  relY: number,
  threshold: number = DEFAULT_THRESHOLD,
  defaultZone: DropZone = DEFAULT_ZONE,
): DropZone {
  if (relX < threshold) return "left";
  if (relX > 1 - threshold) return "right";
  if (relY < threshold) return "top";
  if (relY > 1 - threshold) return "bottom";
  return defaultZone;
}

/** Map a drop zone to a split view layout. */
export function zoneToLayout(zone: DropZone): "horizontal" | "vertical" {
  return zone === "left" || zone === "right" ? "horizontal" : "vertical";
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
