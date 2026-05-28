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
  };
}
