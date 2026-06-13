/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Reorder a contiguous block in `getTabs()` so split tabs match
 * `desiredOrder` left-to-right, without shifting the block's start index.
 */
export function reorderSplitTabsForDesiredOrderImpl<T>(
  getTabs: () => T[],
  moveTabBefore: (tab: T, before: T | null) => void,
  desiredOrder: T[],
): void {
  const n = desiredOrder.length;
  if (n < 2) {
    return;
  }

  const tabsSnapshot = getTabs();
  const indices = desiredOrder.map((t) => tabsSnapshot.indexOf(t));
  if (indices.some((i) => i < 0)) {
    return;
  }
  const firstIdx = Math.min(...indices);

  for (let targetPos = 0; targetPos < n; targetPos++) {
    const tabsArr = getTabs();
    const tab = desiredOrder[targetPos]!;
    const cur = tabsArr.indexOf(tab);
    const goal = firstIdx + targetPos;
    if (cur === goal) {
      continue;
    }

    if (cur < goal) {
      const beforeTab = goal + 1 < tabsArr.length
        ? (tabsArr[goal + 1] as T)
        : null;
      moveTabBefore(tab, beforeTab);
    } else {
      const beforeTab = tabsArr[goal] as T;
      moveTabBefore(tab, beforeTab);
    }
  }
}
