// SPDX-License-Identifier: MPL-2.0
import type { SplitViewContextTab } from "../data/types.ts";

/** Preserve native tab restrictions while extending its two-pane limit. */
export function canOpenContextTabsInSplitView(
  tabs: readonly SplitViewContextTab[],
  maxPanes: number,
): boolean {
  return tabs.length > 0 && tabs.length <= maxPanes &&
    tabs.every((tab) =>
      !tab.pinned && !tab.splitview && !tab.hasAttribute("customizemode")
    );
}
