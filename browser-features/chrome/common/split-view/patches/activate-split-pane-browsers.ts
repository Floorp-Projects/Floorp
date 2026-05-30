/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { getGBrowser, type SplitViewGBrowser, type SplitViewTab } from "../data/types.js";
import { findTabByPanelId } from "../utils/find-tab.js";

/**
 * Subset of browser/components/tabbrowser/AsyncTabSwitcher.sys.mjs we need so
 * every visible split pane gets a loading/active docShell — not only
 * gBrowser.selectedTab (see unloadNonRequiredTabs / shouldActivateDocShell).
 */
type AsyncTabSwitcherPartial = {
  getTabState: (tab: unknown) => number;
  setTabState: (tab: unknown, state: number) => void;
  STATE_UNLOADED: number;
  STATE_LOADING: number;
  STATE_LOADED: number;
  STATE_UNLOADING: number;
};

type GBrowserWithSwitcher = SplitViewGBrowser & {
  _getSwitcher?: () => AsyncTabSwitcherPartial | null | undefined;
};

type LinkedBrowserLike = {
  mDestroyed?: boolean;
  isRemoteBrowser?: boolean;
  docShellIsActive?: boolean;
  renderLayers?: boolean;
  frameLoader?: { remoteTab?: { priorityHint?: boolean } };
  preserveLayers?: (v: boolean) => void;
};

function tabOwnerDocumentHidden(tab: SplitViewTab): boolean {
  const og = (tab as unknown as { ownerGlobal?: { document?: Document } })
    .ownerGlobal;
  return Boolean(og?.document?.hidden);
}

const findTabForPanelId = findTabByPanelId;

/**
 * Tabs in gBrowser._tabLayerCache get docShellIsActive=false from
 * AsyncTabSwitcher.deactivateCachedBackgroundTabs() whenever they are not the
 * selected tab — so split panes look blank even though documents loaded.
 * Do not promote split tabs into that cache; remove them if already present.
 */
function removeSplitPaneTabsFromLayerCache(
  gb: SplitViewGBrowser,
  ids: string[],
): void {
  const cache = (gb as unknown as { _tabLayerCache?: SplitViewTab[] })
    ._tabLayerCache;
  if (!cache?.length) {
    return;
  }
  for (const id of ids) {
    const tab = findTabForPanelId(gb, id);
    if (!tab) {
      continue;
    }
    for (let i = cache.length - 1; i >= 0; i--) {
      if (cache[i] === tab) {
        cache.splice(i, 1);
      }
    }
  }
}

/** Turn compositor + docShell back on for every visible split pane. */
function reviveSplitPanePaneBrowsers(
  gb: SplitViewGBrowser,
  ids: string[],
  logger: ConsoleInstance,
): void {
  for (const id of ids) {
    const tab = findTabForPanelId(gb, id);
    if (!tab?.linkedBrowser) {
      continue;
    }
    const browser = tab.linkedBrowser as unknown as LinkedBrowserLike;
    if (browser.mDestroyed) {
      continue;
    }
    try {
      browser.preserveLayers?.(false);
      browser.docShellIsActive = true;
      browser.renderLayers = true;
      const rt = browser.frameLoader?.remoteTab;
      if (rt) {
        rt.priorityHint = true;
      }
      logger.debug(`[splitPaneWarm] revive display panel=${id}`);
    } catch (e) {
      logger.debug(`[splitPaneWarm] revive failed ${id}: ${e}`);
    }
  }
}

/**
 * `setTabState(LOADING)` asserts `!windowHidden` on the **tab browser window**,
 * and (DEBUG) asserts non-remote tabs never end the call still in LOADING.
 * Never use DevTools `globalThis.document` for that — it may be the toolbox.
 */
function softNudgeBrowserLayers(browser: LinkedBrowserLike): void {
  if (!browser.docShellIsActive) {
    browser.docShellIsActive = true;
  }
  browser.renderLayers = true;
  const rt = browser.frameLoader?.remoteTab;
  if (rt) {
    rt.priorityHint = true;
  }
}

/**
 * After session restore (or on tab switch), Gecko often leaves non-selected
 * split tabs in STATE_UNLOADED or with docShellIsActive=false; they only fully
 * load after being selected again. Nudge the async tab switcher so every
 * `splitViewPanels` tab is treated like a visible pane.
 */
export function ensureSplitPaneTabBrowsersAreWarmed(
  logger: ConsoleInstance,
): void {
  const gb = getGBrowser() as GBrowserWithSwitcher | null;
  if (!gb?.tabpanels) {
    return;
  }

  let ids: string[];
  try {
    ids = gb.tabpanels.splitViewPanels;
  } catch {
    return;
  }
  if (!ids || ids.length < 2) {
    return;
  }

  const root = document?.getElementById("tabbrowser-tabpanels");
  if (!root?.hasAttribute("data-floorp-split")) {
    return;
  }

  const anchorTab = findTabForPanelId(gb, ids[0]!);
  if (!anchorTab || tabOwnerDocumentHidden(anchorTab)) {
    logger.debug(
      "[splitPaneWarm] skip: no anchor tab or browser window document.hidden",
    );
    return;
  }

  const switcher = gb._getSwitcher?.();
  if (
    !switcher ||
    typeof switcher.getTabState !== "function" ||
    typeof switcher.setTabState !== "function"
  ) {
    logger.debug("[splitPaneWarm] no gBrowser._getSwitcher() API; skipping");
    return;
  }

  const {
    STATE_UNLOADED,
    STATE_LOADING,
    STATE_LOADED,
    STATE_UNLOADING,
  } = switcher;
  const getTabState = switcher.getTabState.bind(switcher);
  const setTabState = switcher.setTabState.bind(switcher);

  if (
    typeof STATE_UNLOADED !== "number" ||
    typeof STATE_LOADING !== "number" ||
    typeof STATE_LOADED !== "number" ||
    typeof STATE_UNLOADING !== "number"
  ) {
    logger.debug("[splitPaneWarm] switcher state constants missing");
    return;
  }

  for (const id of ids) {
    const tab = findTabForPanelId(gb, id);
    if (!tab?.linkedBrowser) {
      continue;
    }

    const browser = tab.linkedBrowser as unknown as LinkedBrowserLike;
    if (browser.mDestroyed) {
      continue;
    }

    try {
      const state = getTabState(tab);
      const isRemote = Boolean(browser.isRemoteBrowser);

      if (state === STATE_UNLOADED || state === STATE_UNLOADING) {
        if (isRemote && !tabOwnerDocumentHidden(tab)) {
          setTabState(tab, STATE_LOADING);
          logger.debug(`[splitPaneWarm] setTabState(LOADING) panel=${id}`);
        } else {
          softNudgeBrowserLayers(browser);
          logger.debug(
            `[splitPaneWarm] nudge-only (non-remote or conservative) panel=${id}`,
          );
        }
      } else if (state === STATE_LOADING) {
        softNudgeBrowserLayers(browser);
      } else if (state === STATE_LOADED) {
        if (!browser.docShellIsActive) {
          browser.docShellIsActive = true;
        }
        browser.preserveLayers?.(false);
        const rt = browser.frameLoader?.remoteTab;
        if (rt) {
          rt.priorityHint = true;
        }
      }
    } catch (e) {
      logger.debug(`[splitPaneWarm] panel=${id} error: ${e}`);
    }
  }

  removeSplitPaneTabsFromLayerCache(gb, ids);
  reviveSplitPanePaneBrowsers(gb, ids, logger);

  // The pane that was `selectedTab` immediately before the current selection
  // can still be processed by AsyncTabSwitcher postActions one frame later,
  // often the last id in `splitViewPanels` order after sequential cycling.
  const lastId = ids[ids.length - 1];
  if (lastId) {
    requestAnimationFrame(() => {
      requestAnimationFrame(() => {
        removeSplitPaneTabsFromLayerCache(gb, [lastId]);
        reviveSplitPanePaneBrowsers(gb, [lastId], logger);
      });
    });
  }
}

/**
 * Session restore often leaves split tabs UNLOADED until each is selected once.
 * Re-run warm after delays so switcher state has settled (matches manual console timing).
 */
export function scheduleSplitPaneWarmRetries(logger: ConsoleInstance): void {
  const delays = [0, 80, 250, 800, 2500];
  for (const ms of delays) {
    setTimeout(() => {
      ensureSplitPaneTabBrowsersAreWarmed(logger);
    }, ms);
  }
}

type GBWithSelected = SplitViewGBrowser & {
  selectedTab: SplitViewTab;
};

/**
 * Briefly selects each split tab in `splitViewPanels` order so Gecko runs the
 * normal `requestTab` / load path for every pane, then restores the previous
 * selection. Used after session restore to avoid "click each tab once".
 */
/** Extra hold on the last split tab so remote paint can finish before restoring selection. */
const LAST_SPLIT_PANE_SETTLE_MS = 220;

export function scheduleSequentialSplitTabSelectionForLoad(
  logger: ConsoleInstance,
  stepMs = 25,
): void {
  const gb = getGBrowser() as GBWithSelected | null;
  if (!gb?.tabpanels) {
    return;
  }

  let ids: string[];
  try {
    ids = gb.tabpanels.splitViewPanels;
  } catch {
    return;
  }
  if (!ids || ids.length < 2) {
    return;
  }

  const root = document?.getElementById("tabbrowser-tabpanels");
  if (!root?.hasAttribute("data-floorp-split")) {
    return;
  }

  const anchor = findTabForPanelId(gb, ids[0]!);
  if (!anchor || tabOwnerDocumentHidden(anchor)) {
    return;
  }

  const saved = gb.selectedTab;
  let index = 0;

  const step = (): void => {
    if (tabOwnerDocumentHidden(anchor)) {
      logger.debug("[splitPaneCycle] aborted: window hidden");
      return;
    }

    if (index < ids.length) {
      const id = ids[index]!;
      const tab = findTabForPanelId(gb, id);
      if (tab) {
        gb.selectedTab = tab;
        logger.debug(`[splitPaneCycle] select ${id} (${index + 1}/${ids.length})`);
      }
      index++;
      const aboutToRestore = index === ids.length;
      const delay = aboutToRestore
        ? Math.max(stepMs, LAST_SPLIT_PANE_SETTLE_MS)
        : stepMs;
      setTimeout(step, delay);
      return;
    }

    gb.selectedTab = saved;
    logger.debug("[splitPaneCycle] restored selectedTab");
    ensureSplitPaneTabBrowsersAreWarmed(logger);
    for (const extra of [30, 120, 400, 700]) {
      setTimeout(() => ensureSplitPaneTabBrowsersAreWarmed(logger), extra);
    }
  };

  setTimeout(step, 0);
}
