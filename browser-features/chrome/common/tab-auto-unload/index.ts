/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { onCleanup } from "solid-js";
import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import {
  BYTES_PER_MB,
  CHECK_INTERVAL_MS,
  DEFAULT_SETTINGS,
  DEFAULT_STATS,
  LEGACY_TAB_SLEEP_EXCLUSION_PREF,
  MIN_CHECK_INTERVAL_MS,
  MIN_INACTIVE_MINUTES,
  MIN_MEMORY_THRESHOLD_MB,
  TAB_AUTO_UNLOAD_PREF,
  TAB_AUTO_UNLOAD_STATS_PREF,
  type TabAutoUnloadSettings,
  type TabAutoUnloadStats,
} from "./types.ts";

type BrowserTab = XULElement & {
  undiscardable?: boolean;
  linkedBrowser?: {
    currentURI?: { spec: string };
  };
};

type ExtendedGBrowser = typeof globalThis.gBrowser & {
  addTabsProgressListener: (listener: object) => void;
  removeTabsProgressListener: (listener: object) => void;
  prepareDiscardBrowser: (tab: BrowserTab) => Promise<void>;
  discardBrowser: (tab: BrowserTab) => boolean;
  getTabForBrowser: (browser: nsIBrowser) => BrowserTab | null;
};

/** One entry returned by TabUnloader.getSortedTabs(). */
interface TabUnloaderInfo {
  tab: BrowserTab;
  gBrowser: ExtendedGBrowser;
}

/** The subset of Firefox's TabUnloader this feature uses. */
interface TabUnloaderApi {
  getSortedTabs(minInactiveMs?: number): Promise<TabUnloaderInfo[]>;
  isDiscardable(tabInfo: TabUnloaderInfo): boolean;
}

const TAB_UNLOADER_URLS = [
  "resource:///modules/TabUnloader.sys.mjs",
  "moz-src:///browser/components/tabbrowser/TabUnloader.sys.mjs",
];

/** Returns the value when it is a boolean, otherwise the fallback. */
function readBoolean(value: unknown, fallback: boolean): boolean {
  return typeof value === "boolean" ? value : fallback;
}

/**
 * Returns a finite number clamped to `min`, otherwise the fallback.
 */
function readNumber(value: unknown, fallback: number, min: number): number {
  if (typeof value !== "number" || !Number.isFinite(value)) {
    return fallback;
  }
  return Math.max(min, value);
}

/**
 * Trims, drops empty entries, and removes duplicates while keeping the order
 * the user entered.
 */
export function normalizeExcludePatterns(raw: unknown): string[] {
  if (!Array.isArray(raw)) {
    return [];
  }

  const seen = new Set<string>();
  const patterns: string[] = [];
  for (const value of raw) {
    if (typeof value !== "string") {
      continue;
    }
    const trimmed = value.trim();
    if (!trimmed || seen.has(trimmed)) {
      continue;
    }
    seen.add(trimmed);
    patterns.push(trimmed);
  }
  return patterns;
}

/** Normalizes untrusted pref JSON into safe settings. */
export function sanitizeSettings(raw: unknown): TabAutoUnloadSettings {
  if (typeof raw !== "object" || raw === null) {
    return { ...DEFAULT_SETTINGS, excludePatterns: [] };
  }

  const source = raw as Record<string, unknown>;
  return {
    enabled: readBoolean(source.enabled, DEFAULT_SETTINGS.enabled),
    memoryThresholdMB: readNumber(
      source.memoryThresholdMB,
      DEFAULT_SETTINGS.memoryThresholdMB,
      MIN_MEMORY_THRESHOLD_MB,
    ),
    minInactiveMinutes: readNumber(
      source.minInactiveMinutes,
      DEFAULT_SETTINGS.minInactiveMinutes,
      MIN_INACTIVE_MINUTES,
    ),
    excludePatterns: normalizeExcludePatterns(source.excludePatterns),
  };
}

/** Normalizes untrusted pref JSON into safe stats. */
export function sanitizeStats(raw: unknown): TabAutoUnloadStats {
  if (typeof raw !== "object" || raw === null) {
    return { ...DEFAULT_STATS };
  }

  const source = raw as Record<string, unknown>;
  return {
    lastRunAt: readNumber(source.lastRunAt, DEFAULT_STATS.lastRunAt, 0),
    lastUnloadAt: readNumber(
      source.lastUnloadAt,
      DEFAULT_STATS.lastUnloadAt,
      0,
    ),
    unloadCount: readNumber(
      source.unloadCount,
      DEFAULT_STATS.unloadCount,
      0,
    ),
  };
}

/**
 * Whether the memory check should run now.
 *
 * Pure so the throttle and threshold can be tested without a browser window.
 */
export function shouldCheckMemory(
  settings: TabAutoUnloadSettings,
  residentBytes: number,
  stats: TabAutoUnloadStats,
  now: number,
): boolean {
  if (!settings.enabled) {
    return false;
  }

  const elapsed = now - stats.lastRunAt;
  if (elapsed >= 0 && elapsed < MIN_CHECK_INTERVAL_MS) {
    return false;
  }

  return residentBytes >= settings.memoryThresholdMB * BYTES_PER_MB;
}

/** Returns the lowercased hostname of a URL, or null for non-URLs. */
function getHostname(url: string): string | null {
  try {
    return new URL(url).hostname.toLowerCase();
  } catch {
    return null;
  }
}

/** Builds a case-insensitive regexp from a pattern with `*` wildcards. */
function wildcardToRegExp(pattern: string, prefix = ""): RegExp {
  const escaped = pattern
    .replace(/[.+?^${}()|[\]\\]/g, "\\$&")
    .replace(/\*+/g, ".*");
  return new RegExp(`^${prefix}${escaped}$`, "i");
}

/**
 * Matches a URL against one exclusion pattern.
 *
 * Supported forms:
 * - `example.com` matches the domain and its subdomains
 * - `*.example.com` matches the domain and its subdomains
 * - `https://example.com/path/*` matches a full URL with `*` wildcards
 *
 * A full URL without `*` is treated as a prefix, so
 * `https://mail.example.com` also matches paths below it.
 */
export function matchPattern(url: string, pattern: string): boolean {
  const trimmed = pattern.trim();
  if (!trimmed) {
    return false;
  }

  const lowerPattern = trimmed.toLowerCase();
  const hasScheme = lowerPattern.includes("://");
  const hasPath = lowerPattern.includes("/");

  if (hasScheme || hasPath) {
    // Patterns without a scheme still describe a host and path, so match them
    // against the full URL with an optional scheme.
    const prefix = hasScheme ? "" : "(?:[a-z][a-z0-9+.-]*:)?//";
    const regex = wildcardToRegExp(
      lowerPattern.includes("*") ? lowerPattern : `${lowerPattern}*`,
      prefix,
    );
    return regex.test(url);
  }

  const domain = lowerPattern.startsWith("*.")
    ? lowerPattern.slice(2)
    : lowerPattern;
  const hostname = getHostname(url);
  if (hostname) {
    return hostname === domain || hostname.endsWith(`.${domain}`);
  }

  // Non-URL strings (about:, chrome:, etc.) fall back to a substring match.
  return url.toLowerCase().includes(lowerPattern);
}

/** Whether any exclusion pattern matches the URL. */
export function shouldExcludeUrl(url: string, patterns: string[]): boolean {
  return patterns.some((pattern) => matchPattern(url, pattern));
}

export interface LegacyTabSleepExclusionSettings {
  enabled: boolean;
  patterns: string[];
}

/** Reads the old tab sleep exclusion pref JSON, including its enabled flag. */
export function legacySettings(raw: string): LegacyTabSleepExclusionSettings {
  if (!raw) {
    return { enabled: false, patterns: [] };
  }
  try {
    const parsed: unknown = JSON.parse(raw);
    if (typeof parsed !== "object" || parsed === null) {
      return { enabled: false, patterns: [] };
    }
    const source = parsed as Record<string, unknown>;
    return {
      // The old feature defaulted to enabled, so a missing flag keeps that
      // behavior.
      enabled: readBoolean(source.enabled, true),
      patterns: normalizeExcludePatterns(source.patterns),
    };
  } catch {
    return { enabled: false, patterns: [] };
  }
}

/** Reads the patterns from the old tab sleep exclusion pref JSON. */
export function legacyPatterns(raw: string): string[] {
  return legacySettings(raw).patterns;
}

/** Loads Firefox's TabUnloader from the first URL that resolves. */
function loadTabUnloader(): TabUnloaderApi | null {
  for (const url of TAB_UNLOADER_URLS) {
    try {
      const module = ChromeUtils.importESModule(url) as {
        TabUnloader?: TabUnloaderApi;
      };
      if (module.TabUnloader?.getSortedTabs) {
        return module.TabUnloader;
      }
    } catch {
      // Try the next URL.
    }
  }
  return null;
}

@noraComponent(import.meta.hot)
export default class TabAutoUnload extends NoraComponentBase {
  // These are assigned in init(), not with field initializers. The base class
  // constructor calls init() before derived field initializers run, so any
  // emitted field here would overwrite everything init() just set up. `declare`
  // keeps the type without emitting a runtime field.
  declare private settings: TabAutoUnloadSettings;
  declare private prefObserver: nsIObserver | null;
  declare private progressListener: object | null;
  declare private tabOpenListener: ((event: Event) => void) | null;
  declare private checkTimerId: ReturnType<typeof setInterval> | null;
  declare private checking: boolean;
  declare private disposed: boolean;

  /** Wires up prefs, tab listeners, and the memory check timer. */
  init(): void {
    this.settings = { ...DEFAULT_SETTINGS, excludePatterns: [] };
    this.prefObserver = null;
    this.progressListener = null;
    this.tabOpenListener = null;
    this.checkTimerId = null;
    this.checking = false;
    this.disposed = false;

    this.migrateLegacyPref();
    this.clearStatsUserPref();
    this.settings = this.loadSettings();
    this.setupPrefObserver();
    this.setupTabListeners();
    this.applyToAllTabs();
    this.setupCheckTimer();
    onCleanup(() => this.teardown());
  }

  /**
   * Moves the old Design page patterns into the new pref once.
   *
   * Only an enabled legacy setting is migrated. When it is disabled, the pref
   * is left untouched so the saved patterns are not silently turned into
   * active exclusions.
   *
   * The old pref is cleared after a successful migration so a user who later
   * empties the new exclusion list does not get the old patterns back.
   */
  private migrateLegacyPref(): void {
    try {
      const current = Services.prefs.getStringPref(TAB_AUTO_UNLOAD_PREF, "");
      if (current) {
        return;
      }

      const legacy = Services.prefs.getStringPref(
        LEGACY_TAB_SLEEP_EXCLUSION_PREF,
        "",
      );
      if (!legacy) {
        return;
      }

      const legacySettingsValue = legacySettings(legacy);
      if (!legacySettingsValue.enabled) {
        return;
      }

      Services.prefs.setStringPref(
        TAB_AUTO_UNLOAD_PREF,
        JSON.stringify({
          ...DEFAULT_SETTINGS,
          excludePatterns: legacySettingsValue.patterns,
        }),
      );
      Services.prefs.clearUserPref(LEGACY_TAB_SLEEP_EXCLUSION_PREF);
    } catch (error) {
      console.error("[TabAutoUnload] Failed to migrate legacy pref:", error);
    }
  }

  /** Reads and sanitizes the settings pref. */
  private loadSettings(): TabAutoUnloadSettings {
    try {
      const raw = Services.prefs.getStringPref(TAB_AUTO_UNLOAD_PREF, "");
      if (!raw) {
        return { ...DEFAULT_SETTINGS, excludePatterns: [] };
      }
      return sanitizeSettings(JSON.parse(raw));
    } catch (error) {
      console.error("[TabAutoUnload] Failed to load settings:", error);
      return { ...DEFAULT_SETTINGS, excludePatterns: [] };
    }
  }

  /** Reads and sanitizes the shared throttle state. */
  private loadStats(): TabAutoUnloadStats {
    try {
      const raw = Services.prefs.getStringPref(
        TAB_AUTO_UNLOAD_STATS_PREF,
        "",
      );
      if (!raw) {
        return { ...DEFAULT_STATS };
      }
      return sanitizeStats(JSON.parse(raw));
    } catch (error) {
      console.error("[TabAutoUnload] Failed to load stats:", error);
      return { ...DEFAULT_STATS };
    }
  }

  /**
   * Drops stats left in user prefs by earlier builds.
   *
   * The value is shared throttling state, not a setting, so it must not stay
   * in the user's prefs or prefs.js.
   */
  private clearStatsUserPref(): void {
    try {
      if (Services.prefs.prefHasUserValue(TAB_AUTO_UNLOAD_STATS_PREF)) {
        Services.prefs.clearUserPref(TAB_AUTO_UNLOAD_STATS_PREF);
      }
    } catch (error) {
      console.error("[TabAutoUnload] Failed to clear stats user pref:", error);
    }
  }

  /**
   * Publishes the throttle state that every window shares.
   *
   * Writing to the default branch keeps the value in memory: the pref service
   * backs all chrome windows in this process, so they all see the same
   * `lastRunAt`, but nothing is persisted to prefs.js and `about:config` does
   * not list the value as a user-set pref.
   */
  private saveStats(stats: TabAutoUnloadStats): void {
    try {
      Services.prefs
        .getDefaultBranch("")
        .setStringPref(TAB_AUTO_UNLOAD_STATS_PREF, JSON.stringify(stats));
    } catch (error) {
      console.error("[TabAutoUnload] Failed to save stats:", error);
    }
  }

  /** Re-applies exclusions and the timer when the settings pref changes. */
  private setupPrefObserver(): void {
    this.prefObserver = {
      observe: (_subject: nsISupports, topic: string, data: string) => {
        if (topic !== "nsPref:changed" || data !== TAB_AUTO_UNLOAD_PREF) {
          return;
        }
        this.settings = this.loadSettings();
        this.applyToAllTabs();
        this.setupCheckTimer();
      },
    };

    Services.prefs.addObserver(TAB_AUTO_UNLOAD_PREF, this.prefObserver);
  }

  /** Re-checks single tabs on open and after a navigation. */
  private setupTabListeners(): void {
    const gBrowser = globalThis.gBrowser as ExtendedGBrowser;

    this.tabOpenListener = (event: Event) => {
      const tab = (event as CustomEvent).target as BrowserTab;
      // Give the tab a moment to get its initial URI.
      setTimeout(() => {
        if (!this.disposed) {
          this.updateTabDiscardability(tab);
        }
      }, 100);
    };
    globalThis.addEventListener("TabOpen", this.tabOpenListener);

    this.progressListener = {
      onLocationChange: (browser: nsIBrowser) => {
        if (this.disposed) {
          return;
        }
        // TabsProgressListener passes the browser whose location changed, so
        // only that tab needs re-checking here.
        const tab = (globalThis.gBrowser as ExtendedGBrowser).getTabForBrowser(
          browser,
        );
        if (tab) {
          this.updateTabDiscardability(tab);
        }
      },
    };
    gBrowser.addTabsProgressListener(this.progressListener);
  }

  /** Starts the per-window check timer while the feature is enabled. */
  private setupCheckTimer(): void {
    this.clearCheckTimer();
    if (!this.settings.enabled) {
      return;
    }

    this.checkTimerId = setInterval(() => {
      void this.checkMemory();
    }, CHECK_INTERVAL_MS);
  }

  /** Stops the check timer if it is running. */
  private clearCheckTimer(): void {
    if (this.checkTimerId !== null) {
      clearInterval(this.checkTimerId);
      this.checkTimerId = null;
    }
  }

  /** Re-applies the exclusion state to every open tab. */
  private applyToAllTabs(): void {
    const tabs = globalThis.gBrowser.tabs as BrowserTab[];
    for (const tab of tabs) {
      this.updateTabDiscardability(tab);
    }
  }

  /**
   * Marks or unmarks one tab as undiscardable for the exclusions in effect.
   */
  private updateTabDiscardability(tab: BrowserTab): void {
    const url = this.getTabUrl(tab);
    if (!url) {
      return;
    }

    // Exclusions protect tabs even while automatic unloading is off, because
    // they also apply to Firefox's own low-memory unloader.
    const shouldExclude = shouldExcludeUrl(
      url,
      this.settings.excludePatterns,
    );

    if (shouldExclude) {
      tab.undiscardable = true;
      tab.setAttribute("floorp-auto-unload-excluded", "true");
    } else if (tab.getAttribute("floorp-auto-unload-excluded") === "true") {
      // Only remove our own exclusion; another feature may have set it.
      tab.undiscardable = false;
      tab.removeAttribute("floorp-auto-unload-excluded");
    }
  }

  /** Current URL of a tab, or null while it has no usable URI. */
  private getTabUrl(tab: BrowserTab): string | null {
    try {
      return tab.linkedBrowser?.currentURI?.spec ?? null;
    } catch {
      return null;
    }
  }

  /** Reads resident memory for the parent and every content process. */
  private async readTotalResident(): Promise<number> {
    try {
      const info = await ChromeUtils.requestProcInfo();
      const children = info.children ?? [];
      return children.reduce(
        (total, child) => total + (child.memory ?? 0),
        info.memory ?? 0,
      );
    } catch (error) {
      console.error(
        "[TabAutoUnload] requestProcInfo failed; skipping this check:",
        error,
      );
      return -1;
    }
  }

  /**
   * Checks memory and unloads at most one tab.
   *
   * One tab per interval is deliberate: process memory does not drop
   * immediately, so unloading several tabs from one reading could discard far
   * more than the threshold calls for.
   */
  private async checkMemory(): Promise<void> {
    if (this.checking || this.disposed || !this.settings.enabled) {
      return;
    }
    this.checking = true;

    try {
      const now = Date.now();
      const stats = this.loadStats();
      if (now - stats.lastRunAt < MIN_CHECK_INTERVAL_MS) {
        return;
      }

      // Claim the slot before the asynchronous work so other windows skip it.
      this.saveStats({ ...stats, lastRunAt: now });

      const residentBytes = await this.readTotalResident();
      if (
        this.disposed || !shouldCheckMemory(this.settings, residentBytes, {
          ...stats,
          lastRunAt: 0,
        }, now)
      ) {
        return;
      }

      await this.unloadOneTab(stats, now);
    } finally {
      this.checking = false;
    }
  }

  /** Discards the least recently used tab that is safe to unload. */
  private async unloadOneTab(
    stats: TabAutoUnloadStats,
    now: number,
  ): Promise<void> {
    const tabUnloader = loadTabUnloader();
    if (!tabUnloader) {
      console.error("[TabAutoUnload] TabUnloader is unavailable");
      return;
    }

    const minInactiveMs = Math.round(
      this.settings.minInactiveMinutes * 60_000,
    );

    let tabs: TabUnloaderInfo[];
    try {
      tabs = await tabUnloader.getSortedTabs(minInactiveMs);
    } catch (error) {
      console.error("[TabAutoUnload] getSortedTabs failed:", error);
      return;
    }

    for (const tabInfo of tabs) {
      if (this.disposed) {
        return;
      }
      if (!tabUnloader.isDiscardable(tabInfo)) {
        continue;
      }

      const url = this.getTabUrl(tabInfo.tab);
      if (url && shouldExcludeUrl(url, this.settings.excludePatterns)) {
        continue;
      }

      try {
        await tabInfo.gBrowser.prepareDiscardBrowser(tabInfo.tab);
        if (!tabInfo.gBrowser.discardBrowser(tabInfo.tab)) {
          continue;
        }
      } catch (error) {
        console.error("[TabAutoUnload] Failed to unload tab:", error);
        continue;
      }

      this.saveStats({
        lastRunAt: now,
        lastUnloadAt: now,
        unloadCount: stats.unloadCount + 1,
      });
      console.log("[TabAutoUnload] Unloaded an inactive tab");
      return;
    }
  }

  /** Removes the timer, observers, and listeners when the window closes. */
  private teardown(): void {
    this.disposed = true;
    this.clearCheckTimer();

    if (this.prefObserver) {
      try {
        Services.prefs.removeObserver(
          TAB_AUTO_UNLOAD_PREF,
          this.prefObserver,
        );
      } catch (error) {
        console.error("[TabAutoUnload] Failed to remove pref observer:", error);
      }
      this.prefObserver = null;
    }

    if (this.tabOpenListener) {
      globalThis.removeEventListener("TabOpen", this.tabOpenListener);
      this.tabOpenListener = null;
    }

    if (this.progressListener) {
      try {
        (globalThis.gBrowser as ExtendedGBrowser).removeTabsProgressListener(
          this.progressListener,
        );
      } catch (error) {
        console.error(
          "[TabAutoUnload] Failed to remove progress listener:",
          error,
        );
      }
      this.progressListener = null;
    }
  }
}
