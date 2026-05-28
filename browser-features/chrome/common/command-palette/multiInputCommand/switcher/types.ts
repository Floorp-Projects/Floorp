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

export interface ChromeWindow extends Window {
  gBrowser?: {
    selectedBrowser?: { contentPrincipal?: unknown };
    loadURI?(uri: nsIURI, options: { triggeringPrincipal?: unknown }): void;
  };
}

export interface SqliteRow {
  getResultByName(name: string): string | null;
}

export interface SqliteConnection {
  executeCached(sql: string, params?: unknown[]): Promise<SqliteRow[]>;
  close(): Promise<void>;
}

export interface SqliteModule {
  Sqlite: {
    cloneStorageConnection(connection: {
      connection: unknown;
      readOnly: boolean;
    }): Promise<SqliteConnection>;
  };
}

export interface HistoryPlacesUtilsModule {
  PlacesUtils: {
    history: {
      DBConnection: unknown;
    };
  };
}
