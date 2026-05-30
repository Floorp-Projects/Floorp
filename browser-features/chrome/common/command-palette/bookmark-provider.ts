// SPDX-License-Identifier: MPL-2.0

import type { PaletteCommand, SearchPlacesUtilsModule, UrlLike } from "./types.ts";
import { navigateToUrl } from "./utils/navigate.ts";

const BOOKMARK_COMMAND_PREFIX = "__bookmark__";

function toUrlString(url: UrlLike | null): string {
  if (!url) return "";
  if (typeof url === "string") return url;
  if ("href" in url) return url.href;
  if ("spec" in url) return url.spec;
  return String(url);
}

export async function searchBookmarks(
  query: string,
  limit: number = 10,
): Promise<PaletteCommand[]> {
  try {
    const { PlacesUtils } = ChromeUtils.importESModule(
      "resource://gre/modules/PlacesUtils.sys.mjs",
    ) as SearchPlacesUtilsModule;

    const results = await PlacesUtils.bookmarks.search(query);

    // Use TYPE_BOOKMARK constant if available, otherwise fall back to value 1
    const TYPE_BOOKMARK = PlacesUtils.bookmarks?.TYPE_BOOKMARK ?? 1;

    const seenUrls = new Set<string>();
    const commands: PaletteCommand[] = [];

    for (const item of results) {
      if (item.type !== TYPE_BOOKMARK) continue;
      const urlString = toUrlString(item.url);
      if (!urlString) continue;
      if (seenUrls.has(urlString)) continue;
      seenUrls.add(urlString);

      const title = item.title || urlString;
      commands.push({
        id: `${BOOKMARK_COMMAND_PREFIX}${encodeURIComponent(urlString)}`,
        label: title,
        description: urlString,
        category: "bookmark-suggestions",
        keywords: [urlString, item.title].filter((k): k is string => !!k),
        fn: (win) => navigateToUrl(win, urlString),
      });

      if (commands.length >= limit) break;
    }

    return commands;
  } catch (e) {
    console.error("[command-palette/bookmark] Bookmark search failed:", e);
    return [];
  }
}

export function isBookmarkCommand(id: string): boolean {
  return id.startsWith(BOOKMARK_COMMAND_PREFIX);
}
