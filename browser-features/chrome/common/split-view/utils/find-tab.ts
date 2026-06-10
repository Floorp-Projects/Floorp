/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  getGBrowser,
  type SplitViewGBrowser,
  type SplitViewTab,
} from "../data/types.js";

/**
 * Look up a split-view tab by its linked panel id.
 * Shared by pane-drag, activate-split-pane-browsers, and other modules
 * that need to resolve panel ids to tabs.
 */
export function findTabByPanelId(
  gb: SplitViewGBrowser,
  panelId: string,
): SplitViewTab | undefined {
  for (const t of gb.tabs) {
    if (t.linkedPanel === panelId) {
      return t;
    }
  }
  return undefined;
}

/**
 * Convenience wrapper that resolves gBrowser automatically.
 * Returns `null` when gBrowser is unavailable (e.g. during window teardown).
 */
export function findTabByPanelIdAuto(
  panelId: string,
): SplitViewTab | null {
  const gb = getGBrowser();
  if (!gb?.tabs) {
    return null;
  }
  return findTabByPanelId(gb, panelId) ?? null;
}
