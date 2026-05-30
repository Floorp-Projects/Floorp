// SPDX-License-Identifier: MPL-2.0

import type {
  PaletteCommand,
  SqliteConnection,
  SqliteModule,
  HistoryPlacesUtilsModule,
} from "./types.ts";
import { navigateToUrl } from "./utils/navigate.ts";

const HISTORY_COMMAND_PREFIX = "__history__";

/**
 * Search browsing history asynchronously using Sqlite.sys.mjs.
 * Runs the query on a background thread to avoid main-thread blocking.
 */
export async function searchHistory(
  query: string,
  limit: number = 10,
): Promise<PaletteCommand[]> {
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
    const searchPattern = `%${query}%`;

    const rows = await conn.executeCached(
      `SELECT p.url, p.title,
              MAX(v.visit_date) AS last_visit_date
       FROM moz_places p
       JOIN moz_historyvisits v ON p.id = v.place_id
       WHERE v.visit_date >= ?
         AND (p.title LIKE ? OR p.url LIKE ?)
       GROUP BY p.id
       ORDER BY last_visit_date DESC
       LIMIT ?`,
      [sevenDaysAgoMicroseconds, searchPattern, searchPattern, limit],
    );

    const commands: PaletteCommand[] = [];
    for (const row of rows) {
      const url = row.getResultByName("url") || "";
      const title = row.getResultByName("title") || url;
      commands.push({
        id: `${HISTORY_COMMAND_PREFIX}${encodeURIComponent(url)}`,
        label: title,
        description: url,
        category: "history-suggestions",
        keywords: [url, row.getResultByName("title")].filter(
          (k): k is string => !!k,
        ),
        fn: (win) => navigateToUrl(win, url),
      });
    }

    return commands;
  } catch (e) {
    console.error("[command-palette] History search failed", e);
    return [];
  } finally {
    await conn?.close().catch(() => {});
  }
}

export function isHistoryCommand(id: string): boolean {
  return id.startsWith(HISTORY_COMMAND_PREFIX);
}
