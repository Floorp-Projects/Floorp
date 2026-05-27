// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type {
  PaletteCommand,
  CommandStepChoice,
  StepChoicesResult,
} from "../../command-registry.ts";

/**
 * Get hiragana reading keywords for a given action/command ID from i18n.
 * Returns an empty array for non-Japanese locales or if no readings are defined.
 */
function getJapaneseReadings(id: string): string[] {
  try {
    const readings: unknown = i18next.t(`commandPaletteReadings.${id}`, {
      defaultValue: [] as string[],
      returnObjects: true,
    });
    if (Array.isArray(readings)) {
      return readings.filter((r): r is string => typeof r === "string");
    }
    return [];
  } catch {
    return [];
  }
}

const PAGE_SIZE = 50;
const MAX_PATH_LENGTH = 80;

interface ChromeWindow extends Window {
  gBrowser?: {
    selectedBrowser?: { contentPrincipal?: unknown };
    loadURI?(uri: nsIURI, options: { triggeringPrincipal?: unknown }): void;
  };
}

interface BookmarkTreeNode {
  guid: string;
  title: string;
  index: number;
  dateAdded: number;
  lastModified: number;
  type: string;
  uri?: string;
  children?: BookmarkTreeNode[];
  root?: string;
}

interface PlacesUtilsBookmarks {
  rootGuid: string;
  menuGuid: string;
  toolbarGuid: string;
  unfiledGuid: string;
  mobileGuid: string;
}

interface PlacesUtilsModule {
  PlacesUtils: {
    bookmarks: PlacesUtilsBookmarks;
    promiseBookmarksTree(
      guid: string,
      options?: { includeItemIds?: boolean },
    ): Promise<BookmarkTreeNode | null>;
  };
}

function buildPath(parentPath: string, folderTitle: string): string {
  const fullPath = parentPath ? `${parentPath} > ${folderTitle}` : folderTitle;
  if (fullPath.length > MAX_PATH_LENGTH) {
    return "…" + fullPath.slice(-(MAX_PATH_LENGTH - 1));
  }
  return fullPath;
}

function flattenBookmarks(
  nodes: BookmarkTreeNode[],
  parentPath: string,
  results: CommandStepChoice[],
  maxResults: number,
): void {
  for (const node of nodes) {
    if (results.length >= maxResults) return;

    if (node.type === "text/x-moz-place-container") {
      const folderTitle = node.title || "Untitled";
      const currentPath = buildPath(parentPath, folderTitle);
      if (node.children) {
        flattenBookmarks(node.children, currentPath, results, maxResults);
      }
    } else if (node.type === "text/x-moz-place" && node.uri) {
      results.push({
        label: node.title || node.uri,
        value: node.uri,
        description: parentPath || undefined,
      });
    }
    // Note: text/x-moz-place-separator is intentionally skipped
  }
}

let allFlatBookmarks: CommandStepChoice[] | null = null;

export async function loadBookmarks(): Promise<
  CommandStepChoice[] | StepChoicesResult
> {
  // Reset cache to ensure fresh data on each palette open
  allFlatBookmarks = null;

  try {
    const { PlacesUtils } = ChromeUtils.importESModule(
      "resource://gre/modules/PlacesUtils.sys.mjs",
    ) as PlacesUtilsModule;

    const tree = await PlacesUtils.promiseBookmarksTree(
      PlacesUtils.bookmarks.rootGuid,
    );

    if (!tree?.children) return [];

    const userRoots = tree.children.filter(
      (child: BookmarkTreeNode) =>
        child.guid === PlacesUtils.bookmarks.toolbarGuid ||
        child.guid === PlacesUtils.bookmarks.menuGuid ||
        child.guid === PlacesUtils.bookmarks.unfiledGuid ||
        child.guid === PlacesUtils.bookmarks.mobileGuid,
    );

    const allBookmarks: CommandStepChoice[] = [];
    flattenBookmarks(userRoots, "", allBookmarks, Infinity);

    allFlatBookmarks = allBookmarks;

    const firstPage = allBookmarks.slice(0, PAGE_SIZE);
    const hasMore = allBookmarks.length > PAGE_SIZE;
    let offset = firstPage.length;

    return {
      choices: firstPage,
      hasMore,
      loadMore: hasMore
        ? (): Promise<{
            choices: CommandStepChoice[];
            hasMore: boolean;
          }> => {
            // Defensive: cache may have been invalidated
            if (!allFlatBookmarks) {
              return Promise.resolve({ choices: [], hasMore: false });
            }
            const nextBatch = allFlatBookmarks.slice(
              offset,
              offset + PAGE_SIZE,
            );
            offset += nextBatch.length;
            return Promise.resolve({
              choices: nextBatch,
              hasMore: offset < allFlatBookmarks.length,
            });
          }
        : undefined,
    };
  } catch (err) {
    console.error("[BookmarkSwitcher] Failed to load bookmarks", err);
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
    console.error("[BookmarkSwitcher] Failed to navigate", e);
  }
}

export const bookmarkSwitcherCommand: PaletteCommand = {
  id: "floorp-bookmark-switcher",
  label: i18next.t("commandPalette.bookmarkSwitcher", {
    defaultValue: "Switch Bookmark",
  }),
  description: i18next.t("commandPalette.bookmarkSwitcherDescription", {
    defaultValue: "Open a bookmarked page",
  }),
  category: "switcher",
  keywords: [
    "bookmark",
    "switch bookmark",
    "open bookmark",
    "bookmarks",
    "favorite",
    "favourite",
    ...getJapaneseReadings("floorp-bookmark-switcher"),
  ],
  steps: [
    {
      id: "bookmark",
      label: i18next.t("commandPalette.bookmarkSwitcherStepLabel", {
        defaultValue: "Select a bookmark",
      }),
      placeholder: i18next.t("commandPalette.bookmarkSwitcherStepPlaceholder", {
        defaultValue: "Search or choose a bookmark...",
      }),
      choicesLoader: loadBookmarks,
    },
  ],
  fn: (_win: Window, args?: Record<string, string>) => {
    const url = args?.bookmark;
    if (!url) return;
    navigateToUrl(_win, url);
  },
};
