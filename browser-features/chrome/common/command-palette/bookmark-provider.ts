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
  console.debug(
    "[command-palette/bookmark] searchBookmarks called",
  );
  try {
    const { PlacesUtils } = ChromeUtils.importESModule(
      "resource://gre/modules/PlacesUtils.sys.mjs",
    ) as SearchPlacesUtilsModule;

    console.debug(
      "[command-palette/bookmark] PlacesUtils.bookmarks available:",
      !!PlacesUtils.bookmarks,
    );
    console.debug(
      "[command-palette/bookmark] PlacesUtils.bookmarks.search available:",
      typeof PlacesUtils.bookmarks?.search,
    );
    console.debug(
      "[command-palette/bookmark] TYPE_BOOKMARK value:",
      PlacesUtils.bookmarks?.TYPE_BOOKMARK,
    );

    const results = await PlacesUtils.bookmarks.search(query);
    console.debug(
      "[command-palette/bookmark] Raw results count:",
      results.length,
    );

    // Use TYPE_BOOKMARK constant if available, otherwise fall back to value 1
    const TYPE_BOOKMARK = PlacesUtils.bookmarks?.TYPE_BOOKMARK ?? 1;

    const seenUrls = new Set<string>();
    const commands: PaletteCommand[] = [];

    for (const item of results) {
      if (item.type !== TYPE_BOOKMARK) {
        console.debug(
          "[command-palette/bookmark] Skipping non-bookmark item, type:",
          item.type,
        );
        continue;
      }
      const urlString = toUrlString(item.url);
      if (!urlString) {
        console.debug(
          "[command-palette/bookmark] Skipping item with empty url",
        );
        continue;
      }
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

    console.debug(
      "[command-palette/bookmark] Final commands count:",
      commands.length,
    );
    return commands;
  } catch (e) {
    console.error("[command-palette/bookmark] Bookmark search failed:", e);
    return [];
  }
}

export function isBookmarkCommand(id: string): boolean {
  return id.startsWith(BOOKMARK_COMMAND_PREFIX);
}
