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

// --- Canonical ChromeWindow type (single source of truth) ---

export interface ChromeWindow extends Window {
  gBrowser?: {
    selectedBrowser?: { contentPrincipal?: unknown };
    loadURI?(uri: nsIURI, options: { triggeringPrincipal?: unknown }): void;
  };
}

// --- Bookmark search types (used by bookmark-provider, history-provider) ---

export interface SearchPlacesUtilsModule {
  PlacesUtils: {
    bookmarks: PlacesBookmarks;
  } & {
    history: {
      DBConnection: unknown;
    };
  };
}

// --- Bookmark tree types (used by bookmark-switcher) ---

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

export interface BookmarkTreePlacesUtilsModule {
  PlacesUtils: {
    bookmarks: PlacesUtilsBookmarks;
    promiseBookmarksTree(
      guid: string,
      options?: { includeItemIds?: boolean },
    ): Promise<BookmarkTreeNode | null>;
  };
}

// --- SQLite types (used by history-switcher, history-provider) ---

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

// --- Command Palette Types ---

export interface CommandStepChoice {
  /** Display label shown to the user */
  label: string;
  /** Actual value passed to the command function */
  value: string;
  /** Optional description shown below the label */
  description?: string;
}

export interface StepChoicesResult {
  choices: CommandStepChoice[];
  /** Index of the choice to select by default when the step loads. */
  defaultIndex?: number;
  /** Whether more choices can be loaded via loadMore. */
  hasMore?: boolean;
  /** Load more choices and return the newly loaded choices with updated hasMore flag. */
  loadMore?: () => Promise<{ choices: CommandStepChoice[]; hasMore: boolean }>;
}

export interface CommandStep {
  id: string;
  label: string;
  placeholder: string;
  /**
   * Validate user input for this step.
   * Return `true` to pass, or a string error message to display.
   * Omit to skip validation.
   */
  validate?: (input: string) => boolean | string;
  /**
   * Static predefined choices for this step.
   */
  choices?: CommandStepChoice[];
  /**
   * Dynamic choices loader. Called when entering this step.
   * If both `choices` and `choicesLoader` are defined, `choices` takes priority.
   * Use this for choices that need to be fetched asynchronously (e.g., search engines).
   */
  choicesLoader?: () => Promise<CommandStepChoice[] | StepChoicesResult>;
}

export interface PaletteCommand {
  id: string;
  label: string;
  description: string;
  category: string;
  keywords: string[];
  fn: (win: Window, args?: Record<string, string>) => void;
  /** If defined, the palette will prompt the user for each step before executing fn. */
  steps?: CommandStep[];
}
