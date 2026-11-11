// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type {
  CommandStepChoice,
  PaletteCommand,
  StepChoicesResult,
} from "#features-chrome/common/command-palette/types.ts";
import { getJapaneseReadings } from "#features-chrome/common/command-palette/utils/getJapaneseReadings.ts";
import { getEnglishStepCommandKeywords } from "#features-chrome/common/command-palette/utils/getEnglishKeywords.ts";
import { getSegmentedKeywordsFromI18nKeys } from "#features-chrome/common/command-palette/utils/budouxSegmenter.ts";
import {
  getLiveTabIndex,
  getTabIdentity,
  isTabOpen,
} from "#features-chrome/utils/tab-identity.ts";

interface ChromeWindow extends Window {
  gBrowser?: GBrowser;
}

interface BrowserWithContent extends XULBrowserElement {
  contentTitle?: string;
}

export function loadTabs(
  win: Window = globalThis as unknown as Window,
): Promise<
  CommandStepChoice[] | StepChoicesResult
> {
  try {
    const gBrowser = (win as ChromeWindow).gBrowser;
    if (!gBrowser?.tabs) return Promise.resolve([]);

    const selectedTab = gBrowser.selectedTab;

    let contextualIdentityService: {
      getUserContextLabel(id: number): string;
    } | null = null;
    try {
      const { ContextualIdentityService } = ChromeUtils.importESModule(
        "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
      );
      ContextualIdentityService.ensureDataReady();
      contextualIdentityService = ContextualIdentityService;
    } catch {
      // Container service not available, skip container info
    }

    const choices: CommandStepChoice[] = [];
    let defaultIndex = 0;

    for (const tab of gBrowser.tabs) {
      const browser = tab.linkedBrowser as BrowserWithContent | undefined;
      if (!browser || !isTabOpen(tab)) continue;

      const label = tab.getAttribute("label") || browser.contentTitle || "";
      const url = browser.currentURI?.spec || "";

      let description = url;
      const userContextId = tab.getAttribute("usercontextid");
      if (userContextId && userContextId !== "0" && contextualIdentityService) {
        const containerName = contextualIdentityService.getUserContextLabel(
          Number(userContextId),
        );
        if (containerName) {
          description = `${url} • ${containerName}`;
        }
      }

      if (tab === selectedTab) {
        defaultIndex = choices.length;
      }

      choices.push({
        label,
        value: getTabIdentity(tab),
        description,
      });
    }

    return Promise.resolve({ choices, defaultIndex });
  } catch (err) {
    console.error("[TabSwitcher] Failed to load tabs", err);
    return Promise.resolve([]);
  }
}

export const tabSwitcherCommand: PaletteCommand = {
  id: "floorp-tab-switcher",
  label: i18next.t("commandPalette.tabSwitcher", {
    defaultValue: "Switch Tab",
  }),
  description: i18next.t("commandPalette.tabSwitcherDescription", {
    defaultValue: "Switch to a different tab in the current window",
  }),
  category: "switcher",
  keywords: [
    "switch tab",
    "tab",
    "change tab",
    "go to tab",
    ...getEnglishStepCommandKeywords(
      "commandPalette.tabSwitcher",
      "commandPalette.tabSwitcherDescription",
    ),
    ...getSegmentedKeywordsFromI18nKeys(
      "commandPalette.tabSwitcher",
      "commandPalette.tabSwitcherDescription",
    ),
    ...getJapaneseReadings("floorp-tab-switcher"),
  ],
  steps: [
    {
      id: "tab",
      label: i18next.t("commandPalette.tabSwitcherStepLabel", {
        defaultValue: "Select a tab",
      }),
      placeholder: i18next.t("commandPalette.tabSwitcherStepPlaceholder", {
        defaultValue: "Choose a tab...",
      }),
      choicesLoader: loadTabs,
    },
  ],
  fn: (win: Window, args?: Record<string, string>) => {
    try {
      const tabIdentity = args?.tab;
      if (!tabIdentity) return;

      const gBrowser = (win as ChromeWindow).gBrowser;
      if (!gBrowser?.tabs) return;

      const tabIndex = /^\d+$/.test(tabIdentity) ? Number(tabIdentity) : -1;
      const tab = Number.isSafeInteger(tabIndex) && tabIndex >= 0
        ? gBrowser.tabs[tabIndex]
        : gBrowser.tabs.find((candidate) =>
          getTabIdentity(candidate) === tabIdentity
        );
      if (tab && getLiveTabIndex(gBrowser.tabs, tab) >= 0) {
        gBrowser.selectedTab = tab;
      }
    } catch (err) {
      console.error("[TabSwitcher] Failed to switch tab", err);
    }
  },
};
