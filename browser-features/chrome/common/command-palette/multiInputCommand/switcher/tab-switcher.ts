// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type {
  PaletteCommand,
  CommandStepChoice,
} from "../../command-registry.ts";

interface ChromeWindow extends Window {
  gBrowser?: GBrowser;
}

interface BrowserWithContent extends XULBrowserElement {
  contentTitle?: string;
}

export async function loadTabs(): Promise<CommandStepChoice[]> {
  try {
    const gBrowser = (globalThis as unknown as ChromeWindow).gBrowser;
    if (!gBrowser?.tabs) return [];

    const selectedTab = gBrowser.selectedTab;
    const choices: CommandStepChoice[] = [];

    for (const tab of gBrowser.tabs) {
      if (tab === selectedTab) continue;

      const browser = tab.linkedBrowser as BrowserWithContent | undefined;
      if (!browser) continue;

      const label = tab.getAttribute("label") || browser.contentTitle || "";
      const url = browser.currentURI?.spec || "";

      choices.push({
        label,
        value: String(tab._tPos),
        description: url,
      });
    }

    return choices;
  } catch (err) {
    console.error("[TabSwitcher] Failed to load tabs", err);
    return [];
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
  keywords: ["switch tab", "tab", "change tab", "go to tab"],
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
  fn: (_win: Window, args?: Record<string, string>) => {
    try {
      const tabPos = Number(args?.tab);
      if (isNaN(tabPos)) return;

      const gBrowser = (globalThis as unknown as ChromeWindow).gBrowser;
      if (!gBrowser?.tabs) return;

      const tab = gBrowser.tabs[tabPos];
      if (tab) {
        gBrowser.selectedTab = tab;
      }
    } catch (err) {
      console.error("[TabSwitcher] Failed to switch tab", err);
    }
  },
};
