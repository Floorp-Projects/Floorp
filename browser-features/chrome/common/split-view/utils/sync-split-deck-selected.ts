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

/**
 * After leaving split view, make sure the panel the deck considers selected
 * is displayed.
 *
 * `syncSplitDeckSelectedClasses` takes `.deck-selected` off the normal tab
 * that was shown before a split pane was selected. If the user returns to that
 * tab before Gecko's async tab switcher has shown the pane, the switcher still
 * treats the normal tab as visible (`visibleTab === showTab`) and never calls
 * `updateSelectedIndex()`, so nothing would put the class back and no panel
 * would be shown. The leaving-split cleanup strips the class from the former
 * split panes, so restoring it on the selected panel leaves exactly one
 * displayed panel, which is what the later `updateSelectedIndex()` expects.
 *
 * Returns true when the class had to be added.
 */
export function ensureSelectedPanelDeckSelected(
  selectedPanel: DeckPanelLike | null | undefined,
): boolean {
  if (!selectedPanel || selectedPanel.classList.contains("deck-selected")) {
    return false;
  }
  selectedPanel.classList.add("deck-selected");
  return true;
}
