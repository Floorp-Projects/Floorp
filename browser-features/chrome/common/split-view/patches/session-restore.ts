/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { onCleanup } from "solid-js";
import { splitViewConfig, splitViewPaneSizes } from "../data/config.js";
import {
  getGBrowser,
  PREF_SPLIT_VIEW_SESSION_STATE,
  SPLIT_VIEW_GROUP_ATTRIBUTE,
  SPLIT_VIEW_GROUP_SESSION_KEY,
  SPLIT_VIEW_PANE_INDEX_ATTRIBUTE,
  SPLIT_VIEW_PANE_INDEX_SESSION_KEY,
  type SplitViewLayout,
  type SplitViewPaneSizes,
  type SplitViewTab,
} from "../data/types.js";
import { scheduleSequentialSplitTabSelectionForLoad } from "./activate-split-pane-browsers.js";
import { orderSplitGroupTabsForRestore } from "../utils/order-split-group-tabs.js";
import { collectRestorableSplitGroups } from "../utils/collect-restorable-split-groups.js";
import {
  getGroupLayoutFromStore,
  getGroupPaneSizesFromStore,
  parseGroupLayoutStore,
  upsertGroupLayoutInStore,
  upsertGroupPaneSizesInStore,
} from "../utils/group-layout-store.js";

type SessionStoreWin = {
  promiseInitialized?: Promise<void>;
  promiseAllWindowsRestored?: Promise<void>;
  persistTabAttribute?(name: string): void;
  setCustomTabValue?(tab: XULElement, key: string, value: string): void;
  getCustomTabValue?(tab: XULElement, key: string): string;
  deleteCustomTabValue?(tab: XULElement, key: string): void;
};

function getSessionStore(): SessionStoreWin | null {
  const g = globalThis as typeof globalThis & {
    SessionStore?: SessionStoreWin;
  };
  return g.SessionStore ?? null;
}

const SESSION_WINDOWS_RESTORED_TOPIC = "sessionstore-windows-restored";

function initSessionStoreSplitPersistence(
  ss: SessionStoreWin,
  logger: ConsoleInstance,
): void {
  if (typeof ss.persistTabAttribute === "function") {
    try {
      ss.persistTabAttribute(SPLIT_VIEW_GROUP_ATTRIBUTE);
      ss.persistTabAttribute(SPLIT_VIEW_PANE_INDEX_ATTRIBUTE);
      logger.debug(
        `[session-restore] persistTabAttribute("${SPLIT_VIEW_GROUP_ATTRIBUTE}", "${SPLIT_VIEW_PANE_INDEX_ATTRIBUTE}") registered`,
      );
    } catch (e) {
      logger.error(`[session-restore] persistTabAttribute threw: ${e}`);
    }
    return;
  }
  if (typeof ss.setCustomTabValue === "function") {
    logger.debug(
      `[session-restore] using SessionStore.setCustomTabValue(key="${SPLIT_VIEW_GROUP_SESSION_KEY}") — persistTabAttribute not available`,
    );
    return;
  }
  logger.warn(
    "[session-restore] SessionStore has neither persistTabAttribute nor setCustomTabValue; split group will not persist",
  );
}

export function getSplitViewGroupIdForTab(
  tab: SplitViewTab,
  ss: SessionStoreWin | null = getSessionStore(),
): string | null {
  const el = tabEl(tab);
  const fromAttr = el.getAttribute(SPLIT_VIEW_GROUP_ATTRIBUTE);
  if (fromAttr) {
    return fromAttr;
  }
  if (ss && typeof ss.getCustomTabValue === "function") {
    try {
      const v = ss.getCustomTabValue(el, SPLIT_VIEW_GROUP_SESSION_KEY);
      if (v) {
        return v;
      }
    } catch {
      // ignore
    }
  }
  return null;
}

function setSplitViewGroupOnTab(
  tab: SplitViewTab,
  groupId: string,
  ss: SessionStoreWin | null,
): void {
  const el = tabEl(tab);
  el.setAttribute(SPLIT_VIEW_GROUP_ATTRIBUTE, groupId);
  if (ss && typeof ss.setCustomTabValue === "function") {
    try {
      ss.setCustomTabValue(el, SPLIT_VIEW_GROUP_SESSION_KEY, groupId);
    } catch (e) {
      console.warn("[session-restore] setCustomTabValue failed", e);
    }
  }
}

function getPaneIndexForTab(
  tab: SplitViewTab,
  ss: SessionStoreWin | null,
): number | null {
  const el = tabEl(tab);
  const fromAttr = el.getAttribute(SPLIT_VIEW_PANE_INDEX_ATTRIBUTE);
  if (fromAttr !== null && fromAttr !== "") {
    const n = Number.parseInt(fromAttr, 10);
    if (!Number.isNaN(n)) {
      return n;
    }
  }
  if (ss && typeof ss.getCustomTabValue === "function") {
    try {
      const v = ss.getCustomTabValue(el, SPLIT_VIEW_PANE_INDEX_SESSION_KEY);
      if (v !== undefined && v !== null && String(v) !== "") {
        const n = Number.parseInt(String(v), 10);
        if (!Number.isNaN(n)) {
          return n;
        }
      }
    } catch {
      // ignore
    }
  }
  return null;
}

function setPaneIndexOnTab(
  tab: SplitViewTab,
  index: number,
  ss: SessionStoreWin | null,
): void {
  const el = tabEl(tab);
  el.setAttribute(SPLIT_VIEW_PANE_INDEX_ATTRIBUTE, String(index));
  if (ss && typeof ss.setCustomTabValue === "function") {
    try {
      ss.setCustomTabValue(
        el,
        SPLIT_VIEW_PANE_INDEX_SESSION_KEY,
        String(index),
      );
    } catch (e) {
      console.warn("[session-restore] setCustomTabValue pane index failed", e);
    }
  }
}

function clearPaneIndexOnTab(
  tab: SplitViewTab,
  ss: SessionStoreWin | null,
): void {
  const el = tabEl(tab);
  el.removeAttribute(SPLIT_VIEW_PANE_INDEX_ATTRIBUTE);
  if (ss && typeof ss.deleteCustomTabValue === "function") {
    try {
      ss.deleteCustomTabValue(el, SPLIT_VIEW_PANE_INDEX_SESSION_KEY);
    } catch {
      // ignore
    }
  }
}

function clearSplitViewGroupOnTab(
  tab: SplitViewTab,
  ss: SessionStoreWin | null,
): void {
  const el = tabEl(tab);
  el.removeAttribute(SPLIT_VIEW_GROUP_ATTRIBUTE);
  if (ss && typeof ss.deleteCustomTabValue === "function") {
    try {
      ss.deleteCustomTabValue(el, SPLIT_VIEW_GROUP_SESSION_KEY);
    } catch {
      // ignore
    }
  }
  clearPaneIndexOnTab(tab, ss);
}

function newGroupId(): string {
  return (
    Date.now().toString(36) + Math.random().toString(36).slice(2, 8)
  );
}

function tabEl(tab: SplitViewTab): XULElement {
  return tab as unknown as XULElement;
}

function isEligibleRestoreTab(tab: SplitViewTab): boolean {
  const browser = tab.linkedBrowser as
    | { currentURI?: { spec?: string } }
    | null;
  if (!browser) {
    return false;
  }
  const spec = browser.currentURI?.spec ?? "";
  if (spec === "about:opentabs") {
    return false;
  }
  return true;
}

function readGroupLayoutStore() {
  try {
    return parseGroupLayoutStore(
      Services.prefs.getStringPref(PREF_SPLIT_VIEW_SESSION_STATE, "{}"),
    );
  } catch (e) {
    console.error("[session-restore] readGroupLayoutStore failed", e);
    return { groups: [] };
  }
}

export function getPersistedGroupLayout(
  groupId: string,
): SplitViewLayout | null {
  return getGroupLayoutFromStore(readGroupLayoutStore(), groupId);
}

export function getPersistedGroupPaneSizes(
  groupId: string,
): SplitViewPaneSizes | null {
  return getGroupPaneSizesFromStore(readGroupLayoutStore(), groupId);
}

export function setPersistedGroupLayout(
  groupId: string,
  layout: SplitViewLayout,
): void {
  try {
    const nextStore = upsertGroupLayoutInStore(
      readGroupLayoutStore(),
      groupId,
      layout,
    );
    Services.prefs.setStringPref(
      PREF_SPLIT_VIEW_SESSION_STATE,
      JSON.stringify(nextStore),
    );
  } catch (e) {
    console.error("[session-restore] setPersistedGroupLayout failed", e);
  }
}

export function setPersistedGroupPaneSizes(
  groupId: string,
  paneSizes: SplitViewPaneSizes,
): void {
  try {
    const nextStore = upsertGroupPaneSizesInStore(
      readGroupLayoutStore(),
      groupId,
      paneSizes,
    );
    Services.prefs.setStringPref(
      PREF_SPLIT_VIEW_SESSION_STATE,
      JSON.stringify(nextStore),
    );
  } catch (e) {
    console.error("[session-restore] setPersistedGroupPaneSizes failed", e);
  }
}

export function getSplitViewGroupIdForTabs(
  tabs: SplitViewTab[],
): string | null {
  for (const tab of tabs) {
    const groupId = getSplitViewGroupIdForTab(tab);
    if (groupId) {
      return groupId;
    }
  }
  return null;
}

export function getActiveSplitViewGroupId(): string | null {
  const gBrowser = getGBrowser();
  const tabs = gBrowser?.activeSplitView?.tabs;
  if (!tabs || tabs.length < 2) {
    return null;
  }
  return getSplitViewGroupIdForTabs(tabs);
}

export function resolveLayoutForSplitTabs(
  tabs: SplitViewTab[] | null | undefined,
): SplitViewLayout {
  const fallback = splitViewConfig().layout;
  if (!tabs || tabs.length < 2) {
    return fallback;
  }
  const groupId = getSplitViewGroupIdForTabs(tabs);
  if (!groupId) {
    return fallback;
  }
  return getPersistedGroupLayout(groupId) ?? fallback;
}

export function resolveLayoutForPanelIds(
  panelIds: string[],
): SplitViewLayout {
  const gBrowser = getGBrowser();
  if (!gBrowser?.tabs || panelIds.length < 2) {
    return splitViewConfig().layout;
  }
  const panelSet = new Set(panelIds);
  const tabs = gBrowser.tabs.filter((tab) => panelSet.has(tab.linkedPanel));
  return resolveLayoutForSplitTabs(tabs);
}

export function resolvePaneSizesForSplitTabs(
  tabs: SplitViewTab[] | null | undefined,
): SplitViewPaneSizes {
  const fallback = splitViewPaneSizes();
  if (!tabs || tabs.length < 2) {
    return fallback;
  }
  const groupId = getSplitViewGroupIdForTabs(tabs);
  if (!groupId) {
    return fallback;
  }
  return getPersistedGroupPaneSizes(groupId) ?? fallback;
}

export function resolvePaneSizesForPanelIds(
  panelIds: string[],
): SplitViewPaneSizes {
  const gBrowser = getGBrowser();
  if (!gBrowser?.tabs || panelIds.length < 2) {
    return splitViewPaneSizes();
  }
  const panelSet = new Set(panelIds);
  const tabs = gBrowser.tabs.filter((tab) => panelSet.has(tab.linkedPanel));
  return resolvePaneSizesForSplitTabs(tabs);
}

export function persistPaneSizesForPanelIds(
  panelIds: string[],
  paneSizes: SplitViewPaneSizes,
): void {
  const gBrowser = getGBrowser();
  if (!gBrowser?.tabs || panelIds.length < 2) {
    return;
  }
  const panelSet = new Set(panelIds);
  const tabs = gBrowser.tabs.filter((tab) => panelSet.has(tab.linkedPanel));
  const groupId = getSplitViewGroupIdForTabs(tabs);
  if (!groupId) {
    return;
  }
  setPersistedGroupPaneSizes(groupId, paneSizes);
}

type TabSplitViewActivateDetail = { tabs?: SplitViewTab[] };

/**
 * Ensure split tabs carry `floorpSplitViewGroupId` so SessionStore persists them.
 * Call from TabSplitViewActivate and from patched showSplitViewPanels — upstream
 * does not always dispatch the former when N-pane split is driven from our UI.
 */
export function applySplitViewSessionMarkersForTabs(
  logger: ConsoleInstance,
  tabs: SplitViewTab[],
  source: string,
): void {
  if (!Array.isArray(tabs) || tabs.length < 2) {
    return;
  }

  const ss = getSessionStore();
  let groupId: string | null = null;
  for (const tab of tabs) {
    const existing = getSplitViewGroupIdForTab(tab, ss);
    if (existing) {
      groupId = existing;
      break;
    }
  }
  if (!groupId) {
    groupId = newGroupId();
  }

  for (let i = 0; i < tabs.length; i++) {
    const tab = tabs[i]!;
    setSplitViewGroupOnTab(tab, groupId, ss);
    setPaneIndexOnTab(tab, i, ss);
  }

  const layout = getPersistedGroupLayout(groupId) ?? splitViewConfig().layout;
  setPersistedGroupLayout(groupId, layout);
  const paneSizes = getPersistedGroupPaneSizes(groupId) ?? splitViewPaneSizes();
  setPersistedGroupPaneSizes(groupId, paneSizes);

  logger.debug(
    `[session-restore:markers] source=${source} groupId=${groupId}, tabs=${tabs.length}, layout=${layout}, linkedPanels=[${tabs.map((t) => t.linkedPanel).join(", ")}]`,
  );
}

function onTabSplitViewActivate(logger: ConsoleInstance, e: Event): void {
  const detail = (e as CustomEvent).detail as
    | TabSplitViewActivateDetail
    | undefined;
  const tabs = detail?.tabs;
  if (!Array.isArray(tabs) || tabs.length < 2) {
    return;
  }
  applySplitViewSessionMarkersForTabs(logger, tabs, "TabSplitViewActivate");
}

function onTabSplitViewDeactivate(logger: ConsoleInstance): void {
  const gBrowser = getGBrowser();
  if (!gBrowser?.tabs) {
    return;
  }
  const ss = getSessionStore();
  for (const tab of gBrowser.tabs) {
    if (!tab.splitview) {
      clearSplitViewGroupOnTab(tab, ss);
    }
  }
  logger.debug(
    "[session-restore:deactivate] cleared group id from non-split tabs",
  );
}

function clearSplitViewGroupMarkersExcept(
  allTabs: SplitViewTab[],
  keep: SplitViewTab[],
  ss: SessionStoreWin | null,
): void {
  const keepSet = new Set(keep);
  for (const tab of allTabs) {
    if (!keepSet.has(tab)) {
      clearSplitViewGroupOnTab(tab, ss);
    }
  }
}

function restoreSplitViewFromSession(logger: ConsoleInstance): void {
  const gBrowser = getGBrowser();
  if (!gBrowser?.tabs) {
    logger.debug("[session-restore:restore] skip: no gBrowser.tabs");
    return;
  }

  const splitEnabled = Services.prefs.getBoolPref(
    "browser.tabs.splitView.enabled",
    false,
  );
  if (!splitEnabled) {
    logger.debug(
      "[session-restore:restore] skip: browser.tabs.splitView.enabled=false",
    );
    return;
  }

  // If a split view is already active (e.g. from a previous restore attempt
  // or user action), do not interfere.
  if (gBrowser.activeSplitView) {
    logger.debug(
      "[session-restore:restore] skip: activeSplitView already exists",
    );
    return;
  }

  const allTabs = gBrowser.tabs;
  const ss = getSessionStore();
  logger.debug(
    `[session-restore:restore] scanning ${allTabs.length} tab(s) for split group (attr="${SPLIT_VIEW_GROUP_ATTRIBUTE}" or session key="${SPLIT_VIEW_GROUP_SESSION_KEY}")`,
  );

  const eligibleTabs = allTabs.filter(isEligibleRestoreTab);
  const groupsToRestore = collectRestorableSplitGroups(
    eligibleTabs,
    splitViewConfig().maxPanes,
    (tab) => getSplitViewGroupIdForTab(tab, ss),
    (tabs, tabsInStripOrder) =>
      orderSplitGroupTabsForRestore(
        tabs,
        tabsInStripOrder,
        (tab) => {
          const n = getPaneIndexForTab(tab, ss);
          return n === null ? undefined : n;
        },
      ),
  );

  logger.debug(
    `[session-restore:restore] restorableGroups=${groupsToRestore.map((group) => `${group.groupId}(${group.tabs.length})`).join(", ") || "none"}`,
  );

  if (groupsToRestore.length === 0) {
    logger.debug(
      "[session-restore:restore] no group with 2+ eligible tabs; clearing stray group markers",
    );
    clearSplitViewGroupMarkersExcept(allTabs, [], ss);
    return;
  }

  const restoredTabs: SplitViewTab[] = [];
  const originallySelected = gBrowser.selectedTab;

  for (const group of groupsToRestore) {
    for (let i = 0; i < group.tabs.length; i++) {
      const tab = group.tabs[i]!;
      setSplitViewGroupOnTab(tab, group.groupId, ss);
      setPaneIndexOnTab(tab, i, ss);
    }

    try {
      gBrowser.selectedTab = group.tabs[0]!;
      const wrapper = gBrowser.addTabSplitView(group.tabs, {
        id: group.groupId,
        insertBefore: group.tabs[0],
      });
      logger.debug(
        `[session-restore:restore] addTabSplitView ok: ${group.tabs.length} pane(s), ` +
          `groupId=${group.groupId}, wrapper=${wrapper ? "created" : "null"}, ` +
          `linkedPanels=[${group.tabs.map((t) => t.linkedPanel).join(", ")}]`,
      );
      restoredTabs.push(...group.tabs);
    } catch (e) {
      logger.error(
        `[session-restore:restore] addTabSplitView failed for group=${group.groupId}: ${e}`,
      );
    }
  }

  if (originallySelected) {
    gBrowser.selectedTab = originallySelected;
  }

  setTimeout(() => {
    scheduleSequentialSplitTabSelectionForLoad(logger);
  }, 40);

  clearSplitViewGroupMarkersExcept(allTabs, restoredTabs, ss);
}

export function initSessionRestore(logger: ConsoleInstance): void {
  const tabContainer = getGBrowser()?.tabContainer;
  if (!tabContainer) {
    logger.warn("[session-restore] init skip: no tabContainer");
    return;
  }

  const ss = getSessionStore();
  if (!ss) {
    logger.warn(
      "[session-restore] SessionStore missing; split group session sync + promiseAllWindowsRestored unavailable",
    );
  } else {
    if (ss.promiseInitialized) {
      ss.promiseInitialized.then(() => {
        initSessionStoreSplitPersistence(ss, logger);
      });
    } else {
      initSessionStoreSplitPersistence(ss, logger);
    }
    if (!ss.promiseAllWindowsRestored) {
      logger.warn(
        "[session-restore] SessionStore.promiseAllWindowsRestored missing; using observer only",
      );
    }
  }

  const onActivate = (e: Event): void => {
    onTabSplitViewActivate(logger, e);
  };
  const onDeactivate = (): void => {
    onTabSplitViewDeactivate(logger);
  };

  tabContainer.addEventListener("TabSplitViewActivate", onActivate);
  tabContainer.addEventListener("TabSplitViewDeactivate", onDeactivate);

  /** Coalesce promise + observer so restore runs once after session settles. */
  let restoreTimer: ReturnType<typeof setTimeout> | null = null;
  const scheduleRestoreFromSession = (source: string): void => {
    logger.debug(`[session-restore] scheduleRestore (${source})`);
    if (restoreTimer !== null) {
      clearTimeout(restoreTimer);
    }
    restoreTimer = setTimeout(() => {
      restoreTimer = null;
      restoreSplitViewFromSession(logger);
    }, 0);
  };

  const pRestore = ss?.promiseAllWindowsRestored;
  if (pRestore) {
    pRestore
      .then(() => {
        scheduleRestoreFromSession("promiseAllWindowsRestored");
      })
      .catch((err: Error) => {
        logger.error(`[session-restore] promiseAllWindowsRestored: ${err}`);
        scheduleRestoreFromSession("promiseAllWindowsRestored.catch");
      });
  }

  const windowsRestoredObserver = {
    observe(_subject: unknown, topic: string) {
      if (topic !== SESSION_WINDOWS_RESTORED_TOPIC) {
        return;
      }
      scheduleRestoreFromSession("obs:sessionstore-windows-restored");
    },
  };
  try {
    Services.obs.addObserver(
      windowsRestoredObserver,
      SESSION_WINDOWS_RESTORED_TOPIC,
      false,
    );
  } catch (e) {
    logger.error(`[session-restore] addObserver(sessionstore-windows-restored): ${e}`);
  }

  onCleanup(() => {
    tabContainer.removeEventListener("TabSplitViewActivate", onActivate);
    tabContainer.removeEventListener("TabSplitViewDeactivate", onDeactivate);
    if (restoreTimer !== null) {
      clearTimeout(restoreTimer);
      restoreTimer = null;
    }
    try {
      Services.obs.removeObserver(
        windowsRestoredObserver,
        SESSION_WINDOWS_RESTORED_TOPIC,
      );
    } catch (e) {
      logger.debug(`[session-restore] removeObserver: ${e}`);
    }
  });

  logger.debug(
    "[session-restore] listeners attached (TabSplitView + sessionstore-windows-restored)",
  );
}
