/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

type SplitPanelLike = {
  classList: {
    contains(name: string): boolean;
    remove(...names: string[]): void;
  };
  hasAttribute(name: string): boolean;
  removeAttribute(name: string): void;
};

export function resetSplitPanelPresentationState(
  panel: SplitPanelLike,
  selectedPanel?: SplitPanelLike | null,
  force?: boolean,
): boolean {
  const hasSplitMarker = force ||
    panel.classList.contains("split-view-panel") ||
    panel.classList.contains("split-view-panel-active") ||
    panel.hasAttribute("column");

  if (!hasSplitMarker) {
    return false;
  }

  const isSelected = selectedPanel && panel === selectedPanel;
  panel.classList.remove(
    "split-view-panel",
    "split-view-panel-active",
  );
  if (!isSelected) {
    panel.classList.remove("deck-selected");
  }
  panel.removeAttribute("column");
  panel.removeAttribute("data-floorp-active-pane");
  panel.removeAttribute("data-floorp-drag-source");
  panel.removeAttribute("data-floorp-drop-target");
  return true;
}
