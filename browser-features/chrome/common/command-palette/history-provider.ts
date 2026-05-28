// SPDX-License-Identifier: MPL-2.0

import type { PaletteCommand } from "./command-registry.ts";
import type {
  ChromeWindow,
  PlacesUtilsModule,
} from "./types.ts";

const HISTORY_COMMAND_PREFIX = "__history__";

function navigateToUrl(win: Window, url: string): void {
  try {
    const { gBrowser } = win as ChromeWindow;
    const principal = gBrowser?.selectedBrowser?.contentPrincipal;
    gBrowser?.loadURI?.(Services.io.newURI(url), {
      triggeringPrincipal: principal,
    });
  } catch (e) {
    console.error("[command-palette] History navigation failed", e);
  }
}

export function searchHistory(
  query: string,
  limit: number = 10,
): Promise<PaletteCommand[]> {
  return new Promise<PaletteCommand[]>((resolve) => {
    try {
      const { PlacesUtils } = ChromeUtils.importESModule(
        "resource://gre/modules/PlacesUtils.sys.mjs",
      ) as PlacesUtilsModule;

      const historyQuery = PlacesUtils.history.getNewQuery();
      historyQuery.searchTerms = query;
      historyQuery.beginTime = (Date.now() - 7 * 24 * 60 * 60 * 1000) * 1000;
      historyQuery.endTime = Date.now() * 1000;

      const options = PlacesUtils.history.getNewQueryOptions();
      options.sortingMode =
        Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING ?? 1;
      options.maxResults = limit;

      const result = PlacesUtils.history.executeQuery(historyQuery, options);
      const root = result.root;

      const commands: PaletteCommand[] = [];
      root.containerOpen = true;
      try {
        for (let i = 0; i < root.childCount; i++) {
          const node = root.getChild(i);
          const title = node.title || node.uri;
          commands.push({
            id: `${HISTORY_COMMAND_PREFIX}${encodeURIComponent(node.uri)}`,
            label: title,
            description: node.uri,
            category: "history-suggestions",
            keywords: [node.uri, node.title].filter((k): k is string => !!k),
            fn: (win) => navigateToUrl(win, node.uri),
          });
        }
      } finally {
        root.containerOpen = false;
      }

      resolve(commands);
    } catch (e) {
      console.error("[command-palette] History search failed", e);
      resolve([]);
    }
  });
}

export function isHistoryCommand(id: string): boolean {
  return id.startsWith(HISTORY_COMMAND_PREFIX);
}
