// SPDX-License-Identifier: MPL-2.0

export interface BookmarkTreeNode {
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

export interface PlacesUtilsBookmarks {
  rootGuid: string;
  menuGuid: string;
  toolbarGuid: string;
  unfiledGuid: string;
  mobileGuid: string;
}

export interface PlacesUtilsModule {
  PlacesUtils: {
    bookmarks: PlacesUtilsBookmarks;
    promiseBookmarksTree(
      guid: string,
      options?: { includeItemIds?: boolean },
    ): Promise<BookmarkTreeNode | null>;
  };
}
