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

function readBoolean(value: unknown, fallback: boolean): boolean {
  return typeof value === "boolean" ? value : fallback;
}

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

function getHostname(url: string): string | null {
  try {
    return new URL(url).hostname.toLowerCase();
  } catch {
    return null;
  }
}

function wildcardToRegExp(pattern: string): RegExp {
  const escaped = pattern
    .replace(/[.+?^${}()|[\]\\]/g, "\\$&")
    .replace(/\*/g, ".*");
  return new RegExp(`^${escaped}$`, "i");
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
  const isFullUrl = lowerPattern.includes("://");

  if (isFullUrl) {
    const regex = wildcardToRegExp(
      lowerPattern.includes("*") ? lowerPattern : `${lowerPattern}*`,
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

/** Reads the patterns from the old tab sleep exclusion pref JSON. */
export function legacyPatterns(raw: string): string[] {
  if (!raw) {
    return [];
  }
  try {
    const parsed: unknown = JSON.parse(raw);
    if (typeof parsed !== "object" || parsed === null) {
      return [];
    }
    return normalizeExcludePatterns(
      (parsed as Record<string, unknown>).patterns,
    );
  } catch {
    return [];
  }
}

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

  init(): void {
    this.settings = { ...DEFAULT_SETTINGS, excludePatterns: [] };
    this.prefObserver = null;
    this.progressListener = null;
    this.tabOpenListener = null;
    this.checkTimerId = null;
    this.checking = false;
    this.disposed = false;

    this.migrateLegacyPref();
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
   * The old pref is cleared afterwards so a user who later empties the new
   * exclusion list does not get the old patterns back.
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

      const patterns = legacyPatterns(legacy);
      Services.prefs.setStringPref(
        TAB_AUTO_UNLOAD_PREF,
        JSON.stringify({ ...DEFAULT_SETTINGS, excludePatterns: patterns }),
      );
      Services.prefs.clearUserPref(LEGACY_TAB_SLEEP_EXCLUSION_PREF);
    } catch (error) {
      console.error("[TabAutoUnload] Failed to migrate legacy pref:", error);
    }
  }

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

  private saveStats(stats: TabAutoUnloadStats): void {
    try {
      Services.prefs.setStringPref(
        TAB_AUTO_UNLOAD_STATS_PREF,
        JSON.stringify(stats),
      );
    } catch (error) {
      console.error("[TabAutoUnload] Failed to save stats:", error);
    }
  }

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
      onLocationChange: () => {
        if (!this.disposed) {
          this.applyToAllTabs();
        }
      },
    };
    gBrowser.addTabsProgressListener(this.progressListener);
  }

  private setupCheckTimer(): void {
    this.clearCheckTimer();
    if (!this.settings.enabled) {
      return;
    }

    this.checkTimerId = setInterval(() => {
      void this.checkMemory();
    }, CHECK_INTERVAL_MS);
  }

  private clearCheckTimer(): void {
    if (this.checkTimerId !== null) {
      clearInterval(this.checkTimerId);
      this.checkTimerId = null;
    }
  }

  private applyToAllTabs(): void {
    const tabs = globalThis.gBrowser.tabs as BrowserTab[];
    for (const tab of tabs) {
      this.updateTabDiscardability(tab);
    }
  }

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
