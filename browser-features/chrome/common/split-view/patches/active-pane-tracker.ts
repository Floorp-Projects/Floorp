/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { onCleanup } from "solid-js";
import {
  getGBrowser,
  type SplitViewGBrowser,
  type SplitViewTab,
} from "../data/types.js";
import { ensureSplitPaneTabBrowsersAreWarmed } from "./activate-split-pane-browsers.js";
import { hasLiveSplitPanelsState } from "../utils/live-split-ui.js";

const log = console.createInstance({
  prefix: "nora@split-view:active-pane",
});

let splitPanelClassObserver: MutationObserver | null = null;
let deckResyncRaf = 0;
/** Ignore class mutations caused by our own `syncSplitViewDeckSelectedClass` (avoids observer ↔ sync loops). */
let suppressDeckClassObserver = false;

/**
 * `gBrowser.activeSplitView` is often null during session restore even when
 * `tabpanels.splitViewPanels` is already populated. Build the ordered tab list
 * from panel ids in that case.
 */
function splitTabsForActiveIndicator(
  gBrowser: SplitViewGBrowser,
): SplitViewTab[] | null {
  const wrapper = gBrowser.activeSplitView;
  if (wrapper?.tabs && wrapper.tabs.length >= 2) {
    return wrapper.tabs;
  }
  const hasFloorpSplitAttr = Boolean(
    document?.getElementById("tabbrowser-tabpanels")?.hasAttribute(
      "data-floorp-split",
    ),
  );
  const ids = gBrowser.tabpanels?.splitViewPanels;
  if (!hasLiveSplitPanelsState(ids?.length ?? 0, hasFloorpSplitAttr)) {
    return null;
  }
  const tabs: SplitViewTab[] = [];
  for (const id of ids) {
    let found: SplitViewTab | undefined;
    for (const t of gBrowser.tabs) {
      if (t.linkedPanel === id) {
        found = t;
        break;
      }
    }
    if (found) {
      tabs.push(found);
    }
  }
  return tabs.length >= 2 ? tabs : null;
}

function isMultiPaneSplitUiActive(gBrowser: SplitViewGBrowser): boolean {
  const hasFloorpSplitAttr = Boolean(
    document?.getElementById("tabbrowser-tabpanels")?.hasAttribute(
      "data-floorp-split",
    ),
  );
  return (
    !!gBrowser.activeSplitView ||
    hasLiveSplitPanelsState(
      gBrowser.tabpanels?.splitViewPanels?.length ?? 0,
      hasFloorpSplitAttr,
    )
  );
}

/**
 * In N-pane split, Gecko still uses single-pane deck rules: only one child tends
 * to keep `.deck-selected`, which can dim/hide other panes' subtrees. Put
 * `.deck-selected` on **every** `splitViewPanels` row so none are treated as
 * deck-discarded; strip it only from stale `.split-view-panel` nodes not in the
 * current split list.
 */
export function syncSplitViewDeckSelectedClass(
  gBrowser: SplitViewGBrowser,
): void {
  const ids = gBrowser.tabpanels?.splitViewPanels;
  const root = document?.getElementById("tabbrowser-tabpanels");
  const selected = gBrowser.selectedTab as SplitViewTab | undefined;
  if (!ids || ids.length < 2 || !root || !selected?.linkedPanel) {
    return;
  }
  if (!root.hasAttribute("data-floorp-split")) {
    return;
  }
  const selectedId = selected.linkedPanel;
  let selectedInSplit = false;
  for (const id of ids) {
    if (id === selectedId) {
      selectedInSplit = true;
      break;
    }
  }
  if (!selectedInSplit) {
    return;
  }
  const idSet = new Set(ids);
  suppressDeckClassObserver = true;
  try {
    for (const id of ids) {
      const el = root.querySelector(`#${CSS.escape(id)}`);
      if (!el?.classList.contains("split-view-panel")) {
        continue;
      }
      if (!el.classList.contains("deck-selected")) {
        el.classList.add("deck-selected");
      }
    }
    for (const child of root.children) {
      if (!child.classList.contains("split-view-panel")) {
        continue;
      }
      if (!idSet.has(child.id) && child.classList.contains("deck-selected")) {
        child.classList.remove("deck-selected");
      }
    }
  } finally {
    suppressDeckClassObserver = false;
  }
  log.debug(
    `[syncDeckSelected] allPanels=${ids.length} selectedTab="${selectedId}"`,
  );
}

function scheduleSplitDeckResyncFromObserver(): void {
  if (deckResyncRaf) {
    cancelAnimationFrame(deckResyncRaf);
  }
  deckResyncRaf = requestAnimationFrame(() => {
    deckResyncRaf = 0;
    const gb = getGBrowser();
    const tp = document?.getElementById("tabbrowser-tabpanels");
    if (!gb || !tp?.hasAttribute("data-floorp-split")) {
      return;
    }
    if (!isMultiPaneSplitUiActive(gb)) {
      return;
    }
    syncSplitViewDeckSelectedClass(gb);
  });
}

function attachSplitPanelClassObserver(tabpanels: HTMLElement): void {
  if (splitPanelClassObserver) {
    return;
  }
  splitPanelClassObserver = new MutationObserver((records) => {
    if (suppressDeckClassObserver) {
      return;
    }
    for (const rec of records) {
      if (rec.type !== "attributes" || rec.attributeName !== "class") {
        continue;
      }
      const el = rec.target as HTMLElement;
      if (el.classList?.contains("split-view-panel")) {
        scheduleSplitDeckResyncFromObserver();
        break;
      }
    }
  });
  splitPanelClassObserver.observe(tabpanels, {
    subtree: true,
    attributes: true,
    attributeFilter: ["class"],
  });
}

function detachSplitPanelClassObserver(): void {
  splitPanelClassObserver?.disconnect();
  splitPanelClassObserver = null;
  if (deckResyncRaf) {
    cancelAnimationFrame(deckResyncRaf);
    deckResyncRaf = 0;
  }
}

function clearActivePaneIndicator(): void {
  const tabpanels = document?.getElementById(
    "tabbrowser-tabpanels",
  );
  if (!tabpanels) return;

  for (const el of tabpanels.querySelectorAll(
    "[data-floorp-active-pane]",
  )) {
    el.removeAttribute("data-floorp-active-pane");
  }
}

/**
 * Re-apply `split-view-panel-active` to every panel in `splitViewPanels`.
 * Call after `showSplitViewPanels` when upstream may lag (e.g. session restore).
 */
export function ensureSplitPanelsActiveClassFromState(): void {
  const gBrowser = getGBrowser();
  const ids = gBrowser?.tabpanels?.splitViewPanels;
  const root = document?.getElementById("tabbrowser-tabpanels");
  if (
    !ids ||
    !root ||
    !hasLiveSplitPanelsState(ids.length, root.hasAttribute("data-floorp-split"))
  ) {
    log.debug(
      `[ensureActiveClass] skip ids=${ids?.length ?? "undef"} root=${!!root} floorpSplit=${root?.hasAttribute("data-floorp-split") ?? false}`,
    );
    return;
  }
  let added = 0;
  let skippedNotPanel = 0;
  let missingEl = 0;
  for (const id of ids) {
    const child = root.querySelector(`#${CSS.escape(id)}`);
    if (!child) {
      missingEl++;
      log.debug(
        `[ensureActiveClass] no DOM node for splitViewPanels id=${id}`,
      );
      continue;
    }
    if (!child.classList.contains("split-view-panel")) {
      skippedNotPanel++;
      log.debug(
        `[ensureActiveClass] id=${id} missing .split-view-panel class list=[${[...child.classList].join(", ")}]`,
      );
      continue;
    }
    if (!child.classList.contains("split-view-panel-active")) {
      child.classList.add("split-view-panel-active");
      added++;
    }
  }
  log.debug(
    `[ensureActiveClass] ids=${ids.length} addedActiveClass=${added} missingEl=${missingEl} skippedNotPanel=${skippedNotPanel}`,
  );
}

/** Keep split pane deck-selected state in sync without adding visual emphasis. */
export function refreshActiveSplitPaneIndicator(): void {
  const gBrowser = getGBrowser();
  if (!gBrowser) {
    clearActivePaneIndicator();
    return;
  }

  const splitTabs = splitTabsForActiveIndicator(gBrowser);
  if (!splitTabs) {
    log.debug(
      "[refreshIndicator] no split tabs (no wrapper and splitViewPanels<2) → clear",
    );
    clearActivePaneIndicator();
    return;
  }

  const selectedTab = gBrowser.selectedTab;
  const activeIndex = splitTabs.indexOf(selectedTab);

  log.debug(
    `[refreshIndicator] splitTabs=${splitTabs.length} source=${gBrowser.activeSplitView ? "wrapper" : "splitViewPanels"} ` +
      `activeIndex=${activeIndex} ` +
      `selectedLabel="${(selectedTab as SplitViewTab)?.label?.slice(0, 40) ?? ""}"`,
  );

  if (activeIndex === -1) {
    log.warn(
      "[refreshIndicator] selectedTab not in split tab list → skip split selection sync",
    );
    clearActivePaneIndicator();
    return;
  }

  clearActivePaneIndicator();
  syncSplitViewDeckSelectedClass(gBrowser);
}

/**
 * Tracks split view selection changes and keeps pane presentation state in sync.
 */
export function initActivePaneTracker(
  logger: ConsoleInstance,
): void {
  const tabContainer = getGBrowser()?.tabContainer;
  if (!tabContainer) return;

  const scheduleAfterTabSelect = (): void => {
    requestAnimationFrame(() => {
      log.debug("[schedule] TabSelect+rAF0");
      const gb = getGBrowser();
      if (gb && isMultiPaneSplitUiActive(gb)) {
        ensureSplitPanelsActiveClassFromState();
      }
      refreshActiveSplitPaneIndicator();
      requestAnimationFrame(() => {
        const gb2 = getGBrowser();
        if (gb2 && isMultiPaneSplitUiActive(gb2)) {
          syncSplitViewDeckSelectedClass(gb2);
          ensureSplitPaneTabBrowsersAreWarmed(log);
        }
      });
    });
  };

  const scheduleAfterSplitActivate = (): void => {
    requestAnimationFrame(() => {
      log.debug("[schedule] TabSplitViewActivate+rAF");
      ensureSplitPanelsActiveClassFromState();
      ensureSplitPaneTabBrowsersAreWarmed(log);
      refreshActiveSplitPaneIndicator();
      const gb = getGBrowser();
      if (gb) {
        syncSplitViewDeckSelectedClass(gb);
      }
    });
  };

  const onDeactivate = (): void => {
    clearActivePaneIndicator();
    log.debug("[TabSplitViewDeactivate] cleared stale split pane attrs");
  };

  tabContainer.addEventListener("TabSelect", scheduleAfterTabSelect);
  tabContainer.addEventListener(
    "TabSplitViewActivate",
    scheduleAfterSplitActivate,
  );
  tabContainer.addEventListener(
    "TabSplitViewDeactivate",
    onDeactivate,
  );

  const tp = getGBrowser()?.tabpanels as HTMLElement | null | undefined;
  if (tp) {
    attachSplitPanelClassObserver(tp);
  }

  onCleanup(() => {
    detachSplitPanelClassObserver();
    tabContainer.removeEventListener("TabSelect", scheduleAfterTabSelect);
    tabContainer.removeEventListener(
      "TabSplitViewActivate",
      scheduleAfterSplitActivate,
    );
    tabContainer.removeEventListener(
      "TabSplitViewDeactivate",
      onDeactivate,
    );
  });

  logger.debug(
    "[active-pane-tracker] listeners attached",
  );
}
