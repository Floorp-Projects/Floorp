/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { splitViewConfig } from "../data/config.js";
import { getGBrowser } from "../data/types.js";
import {
  getActiveSplitViewGroupId,
  getPersistedGroupLayout,
} from "../patches/session-restore.js";

/**
 * Enhances the URL bar split view button to show N-pane information.
 *
 * The upstream `updateUrlbarButton` (in tabsplitview.js) only sets
 * `data-active-index` on `#split-view-button`. We additionally set
 * `data-pane-count` and `data-layout` attributes.
 */
export function initToolbarButtonEnhancement(): void {
  const tabContainer = getGBrowser()?.tabContainer;
  if (!tabContainer) return;

  tabContainer.addEventListener("TabSelect", onTabStateChanged);
  tabContainer.addEventListener(
    "TabSplitViewActivate",
    onSplitViewActivate,
  );
  tabContainer.addEventListener(
    "TabSplitViewDeactivate",
    onSplitViewDeactivate,
  );
}

export function destroyToolbarButtonEnhancement(): void {
  const tabContainer = getGBrowser()?.tabContainer;
  if (!tabContainer) return;

  tabContainer.removeEventListener("TabSelect", onTabStateChanged);
  tabContainer.removeEventListener(
    "TabSplitViewActivate",
    onSplitViewActivate,
  );
  tabContainer.removeEventListener(
    "TabSplitViewDeactivate",
    onSplitViewDeactivate,
  );
}

function onSplitViewActivate(e: Event): void {
  const detail = (e as CustomEvent).detail;
  const tabs = detail?.tabs;
  if (!Array.isArray(tabs)) return;

  const count = tabs.length;
  if (!Number.isInteger(count) || count < 2 || count > 4) return;

  updateButton(count);
}

function onTabStateChanged(): void {
  syncButtonFromCurrentState();
}

function onSplitViewDeactivate(): void {
  const button = document?.getElementById("split-view-button");
  if (!button) return;

  button.removeAttribute("data-pane-count");
  button.removeAttribute("data-layout");
}

function syncButtonFromCurrentState(): void {
  const gBrowser = getGBrowser();
  const selectedTab = gBrowser?.selectedTab;
  if (!gBrowser?.activeSplitView || !selectedTab?.splitview) {
    onSplitViewDeactivate();
    return;
  }
  updateButton(gBrowser.activeSplitView.tabs.length);
}

function updateButton(paneCount: number): void {
  const button = document?.getElementById("split-view-button");
  if (!button) return;

  button.setAttribute("data-pane-count", String(paneCount));
  const groupId = getActiveSplitViewGroupId();
  const layout = groupId ? getPersistedGroupLayout(groupId) : null;
  button.setAttribute("data-layout", layout ?? splitViewConfig().layout);
}
