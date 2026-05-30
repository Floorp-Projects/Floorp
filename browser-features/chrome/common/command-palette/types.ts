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
