/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import i18next from "i18next";
import { applyLayout } from "../layout.js";
import { setSplitViewConfig, splitViewConfig } from "../data/config.js";
import type { SplitViewLayout, SplitViewTab } from "../data/types.js";
import { getGBrowser } from "../data/types.js";
import {
  getActiveSplitViewGroupId,
  resolveLayoutForSplitTabs,
  setPersistedGroupLayout,
} from "../patches/session-restore.js";

const log = console.createInstance({ prefix: "nora@split-view-picker" });

const t = (key: string): string => (i18next.t as (k: string) => string)(key);

interface LayoutOption {
  layout: SplitViewLayout;
  i18nKey: string;
  minPanes: number;
  maxPanes?: number;
}

const LAYOUT_OPTIONS: LayoutOption[] = [
  {
    layout: "horizontal",
    i18nKey: "splitView.layoutPicker.horizontal",
    minPanes: 2,
  },
  {
    layout: "vertical",
    i18nKey: "splitView.layoutPicker.vertical",
    minPanes: 2,
  },
  {
    layout: "grid-3pane-left-main",
    i18nKey: "splitView.layoutPicker.grid3paneLeftMain",
    minPanes: 3,
    maxPanes: 3,
  },
  {
    layout: "grid-3pane-right-main",
    i18nKey: "splitView.layoutPicker.grid3paneRightMain",
    minPanes: 3,
    maxPanes: 3,
  },
  {
    layout: "grid-3pane-top-main",
    i18nKey: "splitView.layoutPicker.grid3paneTopMain",
    minPanes: 3,
    maxPanes: 3,
  },
  {
    layout: "grid-3pane-bottom-main",
    i18nKey: "splitView.layoutPicker.grid3paneBottomMain",
    minPanes: 3,
    maxPanes: 3,
  },
  {
    layout: "grid-2x2",
    i18nKey: "splitView.layoutPicker.grid2x2",
    minPanes: 4,
  },
];

export function initLayoutPicker(): void {
  const menu = document?.getElementById("split-view-menu");
  if (!menu) {
    log.warn("[init] #split-view-menu not found");
    return;
  }

  menu.addEventListener("popupshowing", onPopupShowing);
  log.debug("[init] layout picker attached to #split-view-menu");
}

export function destroyLayoutPicker(): void {
  const menu = document?.getElementById("split-view-menu");
  if (!menu) return;

  menu.removeEventListener("popupshowing", onPopupShowing);
  removeFloorpMenuItems(menu);
  log.debug("[destroy] layout picker detached");
}

function onPopupShowing(): void {
  const menu = document?.getElementById("split-view-menu");
  if (!menu) return;

  removeFloorpMenuItems(menu);

  const firstSep = menu.querySelector("menuseparator");

  // If there is no existing separator, create one as a divider between
  // our items and the rest of the menu.  When an existing separator is
  // already present we reuse it as the anchor so we don't end up with
  // two consecutive separators.
  let anchor: Element | null = firstSep;
  if (!anchor) {
    const separator = document?.createXULElement("menuseparator");
    if (separator) {
      separator.className = "floorp-split-view-menu-item";
      menu.appendChild(separator);
      anchor = separator;
    }
  }

  const gBrowser = getGBrowser();
  const activeSplitView = gBrowser?.activeSplitView;
  const currentPaneCount = activeSplitView?.tabs?.length ?? 2;
  const currentLayout = activeSplitView
    ? resolveLayoutForSplitTabs(activeSplitView.tabs)
    : splitViewConfig.value.layout;

  log.debug(
    `[popupShowing] panes=${currentPaneCount}, currentLayout=${currentLayout}, activeSplitView=${!!activeSplitView}`,
  );

  for (const opt of LAYOUT_OPTIONS) {
    if (currentPaneCount < opt.minPanes) continue;
    if (opt.maxPanes !== undefined && currentPaneCount > opt.maxPanes) {
      continue;
    }

    const item = document?.createXULElement("menuitem");
    if (!item) continue;

    item.className = "floorp-split-view-menu-item";
    item.setAttribute("label", t(opt.i18nKey));
    item.setAttribute("type", "radio");

    if (opt.layout === currentLayout) {
      item.setAttribute("checked", "true");
    } else {
      item.removeAttribute("checked");
    }

    item.addEventListener("command", () => {
      log.debug(`[command] switching layout to ${opt.layout}`);
      const activeGroupId = getActiveSplitViewGroupId();
      if (activeGroupId) {
        setPersistedGroupLayout(activeGroupId, opt.layout);
      }
      setSplitViewConfig({ ...splitViewConfig.value, layout: opt.layout });
      applyLayout(log);
    });

    if (anchor) {
      anchor.before(item);
    }
  }

  const maxPanes = splitViewConfig.value.maxPanes;
  if (activeSplitView && currentPaneCount < maxPanes) {
    const addItem = document?.createXULElement("menuitem");
    if (addItem) {
      addItem.className = "floorp-split-view-menu-item";
      addItem.setAttribute("label", t("splitView.layoutPicker.addPane"));
      addItem.addEventListener("command", () => {
        log.debug("[command] adding pane");
        addPaneToActiveSplitView();
      });
      menu.appendChild(addItem);
    }
  }

  if (activeSplitView && currentPaneCount > 2) {
    const removeItem = document?.createXULElement("menuitem");
    if (removeItem) {
      removeItem.className = "floorp-split-view-menu-item";
      removeItem.setAttribute("label", t("splitView.layoutPicker.removePane"));
      removeItem.addEventListener("command", () => {
        log.debug("[command] removing last pane");
        removePaneFromActiveSplitView();
      });
      menu.appendChild(removeItem);
    }
  }
}

function removeFloorpMenuItems(menu: Element): void {
  for (const item of menu.querySelectorAll(".floorp-split-view-menu-item")) {
    item.remove();
  }
}

function addPaneToActiveSplitView(): void {
  const gBrowser = getGBrowser();
  const activeSplitView = gBrowser?.activeSplitView;
  if (!activeSplitView) {
    log.warn("[addPane] no activeSplitView");
    return;
  }

  log.debug(`[addPane] current tabs=${activeSplitView.tabs.length}`);

  // Use about:opentabs to let the user pick a tab for the new pane.
  // The upstream onTabListRowClick was patched (opentabs-splitview.mjs)
  // to use the hosting tab (via browsingContext.embedderElement) instead
  // of gBrowser.selectedTab, so it correctly replaces the opentabs pane
  // even in N-pane split view.
  const newTab = gBrowser.addTrustedTab("about:opentabs");
  log.debug(
    `[addPane] created new tab: linkedBrowser=${!!newTab
      .linkedBrowser}, linkedPanel=${newTab.linkedPanel}`,
  );
  activeSplitView.addTabs([newTab]);
  log.debug(`[addPane] after addTabs: tabs=${activeSplitView.tabs.length}`);
}

function removePaneFromActiveSplitView(): void {
  const gBrowser = getGBrowser();
  if (!gBrowser) return;
  const activeSplitView = gBrowser.activeSplitView;
  if (!activeSplitView || activeSplitView.tabs.length <= 2) {
    log.warn(
      `[removePane] cannot remove: tabs=${activeSplitView?.tabs?.length ?? 0}`,
    );
    return;
  }

  const tabs = activeSplitView.tabs;
  const lastTab = tabs[tabs.length - 1];
  log.debug(
    `[removePane] removing tab: linkedPanel=${lastTab.linkedPanel}, linkedBrowser=${!!lastTab
      .linkedBrowser}`,
  );

  gBrowser.moveTabToSplitView(lastTab, null);

  const remainingTabs = tabs.filter((t: SplitViewTab) => t !== lastTab);
  log.debug(`[removePane] remainingTabs=${remainingTabs.length}`);
  if (remainingTabs.length >= 2) {
    gBrowser.showSplitViewPanels(remainingTabs);
  }
}
