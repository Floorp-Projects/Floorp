/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import {
  DEFAULT_SETTINGS,
  TAB_SLEEP_EXCLUSION_PREF,
  type TabSleepExclusionSettings,
} from "./types.ts";

type BrowserTab = XULElement & {
  undiscardable?: boolean;
  linkedBrowser?: {
    currentURI?: { spec: string };
  };
};

type ExtendedGBrowser = typeof globalThis.gBrowser & {
  addTabsProgressListener: (listener: object) => void;
  getTabForBrowser: (browser: unknown) => BrowserTab | null;
};

@noraComponent(import.meta.hot)
export default class TabSleepExclusion extends NoraComponentBase {
  private settings: TabSleepExclusionSettings = DEFAULT_SETTINGS;
  // Using 'any' for observer since the nsIObserver type doesn't include QueryInterface
  // deno-lint-ignore no-explicit-any
  private prefObserver: any = null;

  init(): void {
    this.loadSettings();
    this.setupPrefObserver();
    this.setupTabListeners();
    this.applyToAllTabs();
  }

  /**
   * Load settings from preferences.
   */
  private loadSettings(): void {
    try {
      const prefValue = Services.prefs.getStringPref(
        TAB_SLEEP_EXCLUSION_PREF,
        "",
      );
      if (prefValue) {
        const parsed = JSON.parse(prefValue);
        this.settings = {
          enabled: parsed.enabled ?? DEFAULT_SETTINGS.enabled,
          patterns: Array.isArray(parsed.patterns)
            ? parsed.patterns
            : DEFAULT_SETTINGS.patterns,
        };
      }
    } catch (error) {
      console.error("[TabSleepExclusion] Failed to load settings:", error);
      this.settings = { ...DEFAULT_SETTINGS };
    }
  }

  /**
   * Setup observer for preference changes.
   */
  private setupPrefObserver(): void {
    this.prefObserver = {
      observe: (_subject: unknown, topic: string, data: string) => {
        if (topic === "nsPref:changed" && data === TAB_SLEEP_EXCLUSION_PREF) {
          this.loadSettings();
          this.applyToAllTabs();
        }
      },
      QueryInterface: ChromeUtils.generateQI([
        "nsIObserver",
        "nsISupportsWeakReference",
      ]),
    };

    Services.prefs.addObserver(TAB_SLEEP_EXCLUSION_PREF, this.prefObserver);
  }

  /**
   * Setup listeners for tab events.
   */
  private setupTabListeners(): void {
    const gBrowser = globalThis.gBrowser as ExtendedGBrowser;

    // Listen for new tabs
    globalThis.addEventListener("TabOpen", (event: Event) => {
      const tab = (event as CustomEvent).target as BrowserTab;
      // Delay to allow tab to load
      setTimeout(() => this.updateTabDiscardability(tab), 100);
    });

    // Listen for tab URL changes
    const listener = {
      onLocationChange: () => {
        // Apply to all tabs on any location change
        // This is simpler and more reliable than trying to get the specific tab
        this.applyToAllTabs();
      },
    };

    gBrowser.addTabsProgressListener(listener);
  }

  /**
   * Apply exclusion settings to all existing tabs.
   */
  private applyToAllTabs(): void {
    const tabs = globalThis.gBrowser.tabs as BrowserTab[];
    for (const tab of tabs) {
      this.updateTabDiscardability(tab);
    }
  }

  /**
   * Update the undiscardable attribute for a single tab.
   */
  private updateTabDiscardability(tab: BrowserTab): void {
    if (!this.settings.enabled) {
      // When disabled, remove our exclusion (but don't override other exclusions)
      if (tab.getAttribute("floorp-sleep-excluded") === "true") {
        tab.undiscardable = false;
        tab.removeAttribute("floorp-sleep-excluded");
      }
      return;
    }

    const url = this.getTabUrl(tab);
    if (!url) {
      return;
    }

    const shouldExclude = this.shouldExcludeUrl(url);

    if (shouldExclude) {
      tab.undiscardable = true;
      tab.setAttribute("floorp-sleep-excluded", "true");
    } else if (tab.getAttribute("floorp-sleep-excluded") === "true") {
      // Only remove our exclusion, don't override other exclusions
      tab.undiscardable = false;
      tab.removeAttribute("floorp-sleep-excluded");
    }
  }

  /**
   * Get the current URL of a tab.
   */
  private getTabUrl(tab: BrowserTab): string | null {
    try {
      return tab.linkedBrowser?.currentURI?.spec ?? null;
    } catch {
      return null;
    }
  }

  /**
   * Check if a URL matches any exclusion pattern.
   */
  private shouldExcludeUrl(url: string): boolean {
    if (this.settings.patterns.length === 0) {
      return false;
    }

    return this.settings.patterns.some((pattern) =>
      this.matchPattern(url, pattern),
    );
  }

  /**
   * Match a URL against a pattern.
   * Supports:
   * - Simple domain matching: "example.com"
   * - Wildcard subdomains: "*.example.com"
   * - Full URL patterns: "https://example.com/path/*"
   */
  private matchPattern(url: string, pattern: string): boolean {
    try {
      // Escape special regex characters except *
      const escaped = pattern
        .replace(/[.+?^${}()|[\]\\]/g, "\\$&")
        .replace(/\*/g, ".*");

      // Create regex that matches the pattern anywhere in the URL
      const regex = new RegExp(escaped, "i");
      return regex.test(url);
    } catch {
      // Fallback to simple includes if regex fails
      return url.toLowerCase().includes(pattern.toLowerCase());
    }
  }

  /**
   * Cleanup when component is destroyed.
   */
  cleanup(): void {
    if (this.prefObserver) {
      Services.prefs.removeObserver(
        TAB_SLEEP_EXCLUSION_PREF,
        this.prefObserver,
      );
      this.prefObserver = null;
    }
  }
}
