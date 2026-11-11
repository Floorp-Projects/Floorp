// SPDX-License-Identifier: MPL-2.0

import type { PaletteCommand } from "#features-chrome/common/command-palette/types.ts";
import {
  getLiveTabIndex,
  getTabIdentity,
  isTabOpen,
} from "#features-chrome/utils/tab-identity.ts";

const TAB_COMMAND_PREFIX = "__tab__";

interface ChromeWindow extends Window {
  gBrowser?: GBrowser;
}

interface BrowserWithContent extends XULBrowserElement {
  contentTitle?: string;
}

export function getTabCommands(win: Window): PaletteCommand[] {
  const { gBrowser } = win as ChromeWindow;
  if (!gBrowser?.tabs) return [];

  const commands: PaletteCommand[] = [];
  for (const tab of gBrowser.tabs) {
    const browser = tab.linkedBrowser as BrowserWithContent | undefined;
    if (!browser || !isTabOpen(tab)) continue;

    const title = tab.getAttribute("label") || browser.contentTitle || "";
    const url = browser.currentURI?.spec || "";
    const tabId = getTabIdentity(tab);

    commands.push({
      id: `${TAB_COMMAND_PREFIX}${tabId}`,
      label: title || url,
      description: url,
      category: "open-tabs",
      keywords: [url, title],
      fn: (w) => {
        const gb = (w as ChromeWindow).gBrowser;
        if (gb && getLiveTabIndex(gb.tabs, tab) >= 0) {
          gb.selectedTab = tab;
        }
      },
    });
  }
  return commands;
}

export function isTabCommand(id: string): boolean {
  return id.startsWith(TAB_COMMAND_PREFIX);
}
