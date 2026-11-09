/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

type DeckPanelLike = {
  id: string;
  classList: {
    contains(name: string): boolean;
    add(name: string): void;
    remove(...names: string[]): void;
  };
};

/**
 * Give `.deck-selected` to every split pane and take it off every other
 * `#tabbrowser-tabpanels` child.
 *
 * The removal matters for panels outside the split. When a split pane is
 * selected while its browser is still loading, Gecko's async tab switcher
 * keeps showing the previous (normal) tab and moves `.deck-selected` later
 * with `updateSelectedIndex()`. That call removes the class only from the
 * first `.deck-selected` child. If the split panes already carry the class
 * and come first in DOM order, the previous tab's panel keeps it. Split view
 * hides that panel, but after unsplitting it is painted over the selected tab
 * as a blank (layer-less) page.
 *
 * Returns the ids of non-split panels whose stale class was removed.
 */
export function syncSplitDeckSelectedClasses(
  children: Iterable<DeckPanelLike>,
  splitPanelIds: readonly string[],
): string[] {
  const idSet = new Set(splitPanelIds);
  const cleared: string[] = [];
  for (const child of children) {
    if (idSet.has(child.id)) {
      if (
        child.classList.contains("split-view-panel") &&
        !child.classList.contains("deck-selected")
      ) {
        child.classList.add("deck-selected");
      }
    } else if (child.classList.contains("deck-selected")) {
      child.classList.remove("deck-selected");
      cleared.push(child.id);
    }
  }
  return cleared;
}
