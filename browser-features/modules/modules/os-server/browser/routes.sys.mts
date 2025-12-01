/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * BrowserInfo API routes for the OS Server
 *
 * Provides access to browser tabs, history, and downloads information.
 */

import type {
  NamespaceBuilder,
  Context as RouterContext,
} from "../router.sys.mts";
import type { Download, HistoryItem, Tab } from "../api-spec/types.ts";
import type { BrowserInfoAPI, DownloadData, HistoryData } from "./types.ts";
import { clampInt } from "../http/utils.sys.mts";

// Lazy import of BrowserInfo module
const BrowserInfoModule = () =>
  ChromeUtils.importESModule(
    "resource://noraneko/modules/os-apis/browser-info/BrowserInfo.sys.mjs",
  ) as { BrowserInfo: BrowserInfoAPI };

// -- Mappers ------------------------------------------------------------------
// Convert runtime data (numbers) to published API types (strings for timestamps)

function mapHistoryItems(items: HistoryData[]): HistoryItem[] {
  return items.map((h) => ({
    url: h.url,
    title: h.title,
    lastVisitDate: String(h.lastVisitDate),
    visitCount: h.visitCount,
  })) as HistoryItem[];
}

function mapDownloads(items: DownloadData[]): Download[] {
  return items.map((d) => ({
    id: d.id,
    url: d.url,
    filename: d.filename,
    status: d.status,
    startTime: String(d.startTime),
  })) as Download[];
}

// -- Route registration -------------------------------------------------------

export function registerBrowserRoutes(api: NamespaceBuilder): void {
  api.namespace("/browser", (b: NamespaceBuilder) => {
    b.get<undefined, Tab[]>("/tabs", async () => {
      const { BrowserInfo } = BrowserInfoModule();
      const tabs = await BrowserInfo.getRecentTabs();
      return { status: 200, body: tabs as Tab[] };
    });

    b.get<undefined, HistoryItem[]>(
      "/history",
      async (ctx: RouterContext<undefined>) => {
        const limit = clampInt(ctx.searchParams.get("limit"), 10, 0, 1000);
        const { BrowserInfo } = BrowserInfoModule();
        const historyRaw = await BrowserInfo.getRecentHistory(limit);
        const history = mapHistoryItems(historyRaw);
        return { status: 200, body: history };
      },
    );

    b.get<undefined, Download[]>(
      "/downloads",
      async (ctx: RouterContext<undefined>) => {
        const limit = clampInt(ctx.searchParams.get("limit"), 10, 0, 1000);
        const { BrowserInfo } = BrowserInfoModule();
        const downloadsRaw = await BrowserInfo.getRecentDownloads(limit);
        const downloads = mapDownloads(downloadsRaw);
        return { status: 200, body: downloads };
      },
    );

    b.get<
      undefined,
      { history: HistoryItem[]; tabs: Tab[]; downloads: Download[] }
    >("/context", async (ctx: RouterContext<undefined>) => {
      const h = clampInt(ctx.searchParams.get("historyLimit"), 10, 0, 1000);
      const d = clampInt(ctx.searchParams.get("downloadLimit"), 10, 0, 1000);
      const { BrowserInfo } = BrowserInfoModule();
      const out = await BrowserInfo.getAllContextData(h, d);
      const mapped = {
        history: mapHistoryItems(out.history),
        tabs: out.tabs as Tab[],
        downloads: mapDownloads(out.downloads),
      };
      return { status: 200, body: mapped };
    });
  });
}
