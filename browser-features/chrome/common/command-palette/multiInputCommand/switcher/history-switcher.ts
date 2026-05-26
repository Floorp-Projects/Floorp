// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type {
  PaletteCommand,
  CommandStepChoice,
} from "../../command-registry.ts";

interface HistoryResultNode {
  uri: string;
  title: string;
  accessCount: number;
}

interface HistoryQueryResult {
  root: HistoryContainer;
}

interface HistoryContainer {
  containerOpen: boolean;
  childCount: number;
  getChild(index: number): HistoryResultNode;
}

interface NavHistoryQuery {
  searchTerms: string;
  beginTime: number;
  endTime: number;
}

interface NavHistoryQueryOptions {
  sortingMode: number;
  maxResults: number;
}

interface PlacesHistory {
  getNewQuery(): NavHistoryQuery;
  getNewQueryOptions(): NavHistoryQueryOptions;
  executeQuery(
    query: NavHistoryQuery,
    options: NavHistoryQueryOptions,
  ): HistoryQueryResult;
}

interface PlacesUtilsModule {
  history: PlacesHistory;
}

interface ChromeWindow extends Window {
  gBrowser?: {
    selectedBrowser?: { contentPrincipal?: unknown };
    loadURI?(uri: nsIURI, options: { triggeringPrincipal?: unknown }): void;
  };
}

export async function loadHistory(): Promise<CommandStepChoice[]> {
  try {
    const { PlacesUtils } = ChromeUtils.importESModule(
      "resource://gre/modules/PlacesUtils.sys.mjs",
    ) as PlacesUtilsModule;

    const historyQuery = PlacesUtils.history.getNewQuery();
    historyQuery.beginTime = (Date.now() - 7 * 24 * 60 * 60 * 1000) * 1000; // Last 7 days, in microseconds
    historyQuery.endTime = Date.now() * 1000;

    const options = PlacesUtils.history.getNewQueryOptions();
    options.sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING;
    options.maxResults = 50;

    const result = PlacesUtils.history.executeQuery(historyQuery, options);
    const root = result.root;

    const choices: CommandStepChoice[] = [];
    root.containerOpen = true;
    try {
      for (let i = 0; i < root.childCount; i++) {
        const node = root.getChild(i);
        choices.push({
          label: node.title || node.uri,
          value: node.uri,
          description: node.uri,
        });
      }
    } finally {
      root.containerOpen = false;
    }

    return choices;
  } catch (e) {
    console.error("[HistorySwitcher] Failed to load history", e);
    return [];
  }
}

function navigateToUrl(win: Window, url: string): void {
  try {
    const { gBrowser } = win as unknown as ChromeWindow;
    const principal = gBrowser?.selectedBrowser?.contentPrincipal;
    gBrowser?.loadURI?.(Services.io.newURI(url), {
      triggeringPrincipal: principal,
    });
  } catch (e) {
    console.error("[HistorySwitcher] Failed to navigate", e);
  }
}

export const historySwitcherCommand: PaletteCommand = {
  id: "floorp-history-switcher",
  label: i18next.t("commandPalette.historySwitcher", {
    defaultValue: "Browse History",
  }),
  description: i18next.t("commandPalette.historySwitcherDescription", {
    defaultValue: "Open a page from your browsing history",
  }),
  category: "switcher",
  keywords: [
    "history",
    "browse history",
    "recent pages",
    "visited",
    "open history",
  ],
  steps: [
    {
      id: "history",
      label: i18next.t("commandPalette.historySwitcherStepLabel", {
        defaultValue: "Select a history entry",
      }),
      placeholder: i18next.t("commandPalette.historySwitcherStepPlaceholder", {
        defaultValue: "Choose a history entry...",
      }),
      choicesLoader: loadHistory,
    },
  ],
  fn: (_win: Window, args?: Record<string, string>) => {
    const url = args?.history;
    if (!url) return;
    navigateToUrl(_win, url);
  },
};
