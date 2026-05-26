// SPDX-License-Identifier: MPL-2.0

import type { PaletteCommand } from "./command-registry.ts";

const BOOKMARK_COMMAND_PREFIX = "__bookmark__";

interface ChromeWindow extends Window {
  gBrowser?: {
    selectedBrowser?: { contentPrincipal?: unknown };
    loadURI?(uri: nsIURI, options: { triggeringPrincipal?: unknown }): void;
  };
}

/** Firefox may return url as a string, URL object, or nsIURI */
type UrlLike = string | { href: string } | { spec: string };

interface BookmarkItem {
  guid: string;
  title: string | null;
  url: UrlLike | null;
  type: number;
  parentGuid: string;
  dateAdded: number | null;
  lastModified: number | null;
}

interface PlacesBookmarks {
  search(query: string): Promise<BookmarkItem[]>;
  TYPE_BOOKMARK: number;
}

interface PlacesUtilsModule {
  bookmarks: PlacesBookmarks;
}

function navigateToUrl(win: Window, url: string): void {
  try {
    const { gBrowser } = win as ChromeWindow;
    const principal = gBrowser?.selectedBrowser?.contentPrincipal;
    gBrowser?.loadURI?.(Services.io.newURI(url), {
      triggeringPrincipal: principal,
    });
  } catch (e) {
    console.error("[command-palette] Bookmark navigation failed", e);
  }
}

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
    "[command-palette/bookmark] searchBookmarks called with query:",
    query,
  );
  try {
    const { PlacesUtils } = ChromeUtils.importESModule(
      "resource://gre/modules/PlacesUtils.sys.mjs",
    ) as PlacesUtilsModule;

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

    if (results.length > 0) {
      console.debug(
        "[command-palette/bookmark] First raw result:",
        JSON.stringify(results[0], null, 2),
      );
      console.debug(
        "[command-palette/bookmark] First result type:",
        typeof results[0].type,
        results[0].type,
      );
      console.debug(
        "[command-palette/bookmark] First result url type:",
        typeof results[0].url,
      );
    }

    // Use TYPE_BOOKMARK constant if available, otherwise fall back to value 1
    const TYPE_BOOKMARK = PlacesUtils.bookmarks?.TYPE_BOOKMARK ?? 1;

    const seenUrls = new Set<string>();
    const commands: PaletteCommand[] = [];

    for (const item of results) {
      if (item.type !== TYPE_BOOKMARK) {
        console.debug(
          "[command-palette/bookmark] Skipping non-bookmark item, type:",
          item.type,
          "title:",
          item.title,
        );
        continue;
      }
      const urlString = toUrlString(item.url);
      if (!urlString) {
        console.debug(
          "[command-palette/bookmark] Skipping item with empty url, raw url:",
          item.url,
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
