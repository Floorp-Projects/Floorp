/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export type SplitViewLayout =
  | "horizontal"
  | "vertical"
  | "grid-2x2"
  | "grid-3pane-left-main"
  | "grid-3pane-right-main"
  | "grid-3pane-top-main"
  | "grid-3pane-bottom-main";

// ===== Firefox/Gecko API types for split-view =====

/** A Firefox tab relevant to split view operations. */
export interface SplitViewTab {
  linkedBrowser: XULElement | null;
  linkedPanel: string;
  splitview: XULElement | null;
  selected: boolean;
  label: string;
}

/** The tab-split-view-wrapper custom element. */
export interface SplitViewWrapper {
  tabs: SplitViewTab[];
  addTabs(tabs: SplitViewTab[]): void;
  reverseTabs(): void;
}

/** MozTabpanels custom element with split-view properties. */
export interface MozTabpanels extends XULElement {
  splitViewPanels: string[];
  isSplitViewActive: XULElement | null;
}

/** gBrowser subset used by split-view code. */
export interface SplitViewGBrowser {
  tabpanels: MozTabpanels | null;
  tabContainer: XULElement | null;
  /** All tabs in this window, in visual order. */
  tabs: SplitViewTab[];
  selectedTab: SplitViewTab;
  /** All multi-selected tabs (Ctrl+click / Shift+click). */
  selectedTabs: SplitViewTab[];
  activeSplitView: SplitViewWrapper | null;
  showSplitViewPanels(tabs: SplitViewTab[]): void;
  moveTabBefore(tab: SplitViewTab, beforeTab: SplitViewTab | null): void;
  moveTabToSplitView(tab: SplitViewTab, wrapper: SplitViewWrapper | null): void;
  addTrustedTab(url: string): SplitViewTab;
  /** Create a split view wrapper, move tabs into it, and activate. */
  addTabSplitView(
    tabs: SplitViewTab[],
    options?: { id?: string | null; insertBefore?: SplitViewTab | null },
  ): SplitViewWrapper | null;
}

/** TabContextMenu global. */
export interface TabContextMenuGlobal {
  contextTab: SplitViewTab & { multiselected?: boolean };
  contextTabs: SplitViewTab[];
}

/** Type-safe accessor for gBrowser. */
export function getGBrowser(): SplitViewGBrowser | null {
  return (
    ((globalThis as Record<string, unknown>)
      .gBrowser as SplitViewGBrowser | null) ?? null
  );
}

/** Type-safe accessor for TabContextMenu. */
export function getTabContextMenu(): TabContextMenuGlobal | null {
  return (
    ((globalThis as Record<string, unknown>)
      .TabContextMenu as TabContextMenuGlobal | null) ?? null
  );
}

// ===== Split-view configuration types =====

export interface SplitViewConfig {
  layout: SplitViewLayout;
  maxPanes: number;
}

export interface SplitViewPaneSizes {
  /** For flex layouts: ratio per pane, e.g. [0.5, 0.5] or [0.33, 0.33, 0.34] */
  flexRatios: number[];
  /** For grid layout: column split ratio (0-1) */
  gridColRatio: number;
  /** For grid layout: row split ratio (0-1) */
  gridRowRatio: number;
}

export const DEFAULT_CONFIG: SplitViewConfig = {
  layout: "horizontal",
  maxPanes: 4,
};

export const DEFAULT_PANE_SIZES: SplitViewPaneSizes = {
  flexRatios: [0.5, 0.5],
  gridColRatio: 0.5,
  gridRowRatio: 0.5,
};

export const PREF_SPLIT_VIEW_CONFIG = "floorp.splitView.config";
export const PREF_SPLIT_VIEW_PANE_SIZES = "floorp.splitView.paneSizes";
/** JSON: { groups: { groupId: string; layout: SplitViewLayout }[] } — per-group layout for future multi-split. */
export const PREF_SPLIT_VIEW_SESSION_STATE = "floorp.splitView.sessionState";

/** Session-persisted DOM attribute: tabs sharing one split view use the same value. */
export const SPLIT_VIEW_GROUP_ATTRIBUTE = "floorpSplitViewGroupId";

/**
 * SessionStore.setCustomTabValue key for the same group id (modern Firefox has no
 * persistTabAttribute on window.SessionStore).
 */
export const SPLIT_VIEW_GROUP_SESSION_KEY = "floorp.splitViewGroupId";

/** 0-based pane order within the split (left-to-right / grid slot order). */
export const SPLIT_VIEW_PANE_INDEX_ATTRIBUTE = "floorpSplitViewPaneIndex";

export const SPLIT_VIEW_PANE_INDEX_SESSION_KEY = "floorp.splitViewPaneIndex";
