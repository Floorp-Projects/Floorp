// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type {
  PaletteCommand,
  CommandStepChoice,
  StepChoicesResult,
  SqliteConnection,
  SqliteModule,
  HistoryPlacesUtilsModule,
} from "../../types.ts";
import { getJapaneseReadings } from "../../utils/getJapaneseReadings.ts";
import { navigateToUrl } from "../../utils/navigate.ts";

const PAGE_SIZE = 20;

/**
 * Query browsing history from the Places database asynchronously using Sqlite.sys.mjs.
 * This runs the query on a background thread, avoiding main-thread blocking.
 */
async function queryHistory(
  offset: number,
  limit: number,
): Promise<CommandStepChoice[]> {
  let conn: SqliteConnection | undefined;
  try {
    const { PlacesUtils } = ChromeUtils.importESModule(
      "resource://gre/modules/PlacesUtils.sys.mjs",
    ) as HistoryPlacesUtilsModule;

    const { Sqlite } = ChromeUtils.importESModule(
      "resource://gre/modules/Sqlite.sys.mjs",
    ) as SqliteModule;

    conn = await Sqlite.cloneStorageConnection({
      connection: PlacesUtils.history.DBConnection,
      readOnly: true,
    });

    if (!conn) return [];

    const sevenDaysAgoMicroseconds =
      (Date.now() - 7 * 24 * 60 * 60 * 1000) * 1000;

    const rows = await conn.executeCached(
      `SELECT p.url, p.title, p.visit_count,
              MAX(v.visit_date) AS last_visit_date
       FROM moz_places p
       JOIN moz_historyvisits v ON p.id = v.place_id
       WHERE v.visit_date >= ?
       GROUP BY p.id
       ORDER BY last_visit_date DESC
       LIMIT ? OFFSET ?`,
      [sevenDaysAgoMicroseconds, limit, offset],
    );

    return rows.map((row) => ({
      label: row.getResultByName("title") || row.getResultByName("url") || "",
      value: row.getResultByName("url") || "",
      description: row.getResultByName("url") || "",
    }));
  } catch (e) {
    console.error("[HistorySwitcher] Failed to query history", e);
    return [];
  } finally {
    await conn?.close().catch(() => {});
  }
}

/**
 * Creates the initial history loader that returns the first page of results
 * with pagination support via loadMore callback.
 */
export function loadHistory(): Promise<
  CommandStepChoice[] | StepChoicesResult
> {
  let offset = 0;

  // We use a different approach: return StepChoicesResult with loadMore
  // The hasMore flag is determined by checking if we got exactly PAGE_SIZE results
  return (async (): Promise<StepChoicesResult> => {
    const firstPage = await queryHistory(0, PAGE_SIZE + 1);
    const hasExtra = firstPage.length > PAGE_SIZE;
    if (hasExtra) {
      firstPage.pop();
    }
    offset = firstPage.length;

    return {
      choices: firstPage,
      hasMore: hasExtra,
      loadMore: hasExtra
        ? async (): Promise<{
            choices: CommandStepChoice[];
            hasMore: boolean;
          }> => {
            const nextPage = await queryHistory(offset, PAGE_SIZE + 1);
            const hasMoreResults = nextPage.length > PAGE_SIZE;
            if (hasMoreResults) {
              nextPage.pop();
            }
            offset += nextPage.length;

            return { choices: nextPage, hasMore: hasMoreResults };
          }
        : undefined,
    };
  })();
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
    ...getJapaneseReadings("floorp-history-switcher"),
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
    navigateToUrl(_win, url, "[HistorySwitcher]");
  },
};
