// SPDX-License-Identifier: MPL-2.0

/** Firefox may return url as a string, URL object, or nsIURI */
export type UrlLike = string | { href: string } | { spec: string };

export interface BookmarkItem {
  guid: string;
  title: string | null;
  url: UrlLike | null;
  type: number;
  parentGuid: string;
  dateAdded: number | null;
  lastModified: number | null;
}

export interface PlacesBookmarks {
  search(query: string): Promise<BookmarkItem[]>;
  TYPE_BOOKMARK: number;
}

export interface PlacesUtilsModule {
  PlacesUtils: {
    bookmarks: PlacesBookmarks;
  } & {
    history: PlacesHistory;
  };
}

export interface PlacesHistory {
  getNewQuery(): NavHistoryQuery;
  getNewQueryOptions(): NavHistoryQueryOptions;
  executeQuery(
    query: NavHistoryQuery,
    options: NavHistoryQueryOptions,
  ): HistoryQueryResult;
}

export interface NavHistoryQuery {
  searchTerms: string;
  beginTime: number;
  endTime: number;
}

export interface NavHistoryQueryOptions {
  sortingMode: number;
  maxResults: number;
}

// --- History Provider Types ---

export interface ChromeWindow extends Window {
  gBrowser?: {
    selectedBrowser?: { contentPrincipal?: unknown };
    loadURI?(uri: nsIURI, options: { triggeringPrincipal?: unknown }): void;
  };
}

export interface HistoryResultNode {
  uri: string;
  title: string;
  accessCount: number;
}

export interface HistoryQueryResult {
  root: HistoryContainer;
}

export interface HistoryContainer {
  containerOpen: boolean;
  childCount: number;
  getChild(index: number): HistoryResultNode;
}
