/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Settings for automatic tab unloading.
 *
 * Firefox already knows which tabs are safe to discard (pinned tabs, tabs
 * playing audio, tabs in a call, private tabs, and so on). This feature only
 * decides *when* to ask Firefox to discard one, based on the browser's total
 * memory use and how long a tab has been inactive.
 */
export interface TabAutoUnloadSettings {
  /** Whether automatic unloading is enabled. */
  enabled: boolean;
  /** Unload a tab when total browser memory reaches this many megabytes. */
  memoryThresholdMB: number;
  /** Only consider tabs that have been inactive for this many minutes. */
  minInactiveMinutes: number;
  /**
   * URL patterns whose tabs must never be unloaded. Matching tabs are marked
   * `undiscardable`, so Firefox's own low-memory unloader skips them too.
   */
  excludePatterns: string[];
}

/**
 * Shared state for the per-window feature instances.
 *
 * Every browser window instantiates the chrome component, so this lives in a
 * string pref. `lastRunAt` doubles as the cross-window lock: a window claims it
 * before doing any asynchronous work, and the other windows bail at their own
 * throttle check.
 */
export interface TabAutoUnloadStats {
  /** When the last memory check started, in epoch milliseconds. */
  lastRunAt: number;
  /** When a tab was last unloaded, in epoch milliseconds. */
  lastUnloadAt: number;
  /** How many tabs this feature has unloaded. */
  unloadCount: number;
}

export const TAB_AUTO_UNLOAD_PREF = "floorp.tabs.autoUnload";
export const TAB_AUTO_UNLOAD_STATS_PREF = "floorp.tabs.autoUnload.stats";

/** Pref written by the old Design page feature. */
export const LEGACY_TAB_SLEEP_EXCLUSION_PREF = "floorp.tabs.sleep.exclusion";

export const DEFAULT_SETTINGS: TabAutoUnloadSettings = {
  enabled: false,
  memoryThresholdMB: 2048,
  minInactiveMinutes: 10,
  excludePatterns: [],
};

export const DEFAULT_STATS: TabAutoUnloadStats = {
  lastRunAt: 0,
  lastUnloadAt: 0,
  unloadCount: 0,
};

/** Floors shared with the settings page. */
export const MIN_MEMORY_THRESHOLD_MB = 0;
export const MIN_INACTIVE_MINUTES = 1;

/** How often each window checks memory. */
export const CHECK_INTERVAL_MS = 30_000;

/** Minimum time between checks across all windows. */
export const MIN_CHECK_INTERVAL_MS = 30_000;

export const BYTES_PER_MB = 1024 * 1024;
