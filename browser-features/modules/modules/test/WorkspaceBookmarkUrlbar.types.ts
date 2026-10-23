import type {
  WorkspaceBookmarkContext,
  WorkspaceBookmarksService,
} from "../WorkspaceBookmarks.sys.mts";

export interface BookmarkFixture {
  guid: string;
}

export interface NativePlaces {
  bookmarks: {
    unfiledGuid: string;
    TYPE_FOLDER: number;
    insert(input: {
      parentGuid: string;
      type?: number;
      title: string;
      url?: string;
    }): Promise<BookmarkFixture>;
    remove(guid: string): Promise<void>;
  };
  history: {
    TRANSITIONS: { TYPED: number };
    insert(input: {
      url: string;
      title: string;
      visits: Array<{ date: Date; transition: number }>;
    }): Promise<unknown>;
    remove(urls: string[]): Promise<unknown>;
  };
  keywords: {
    insert(input: { keyword: string; url: string }): Promise<void>;
    remove(keyword: string): Promise<void>;
  };
  promiseLargeCacheDBConnection(): Promise<PlacesConnection>;
  withConnectionWrapper<T>(
    name: string,
    task: (connection: PlacesConnection) => Promise<T>,
  ): Promise<T>;
}

export interface PlacesConnection {
  execute(
    query: string,
    parameters?: Record<string, string | number | null>,
  ): Promise<PlacesRow[]>;
}

export interface PlacesRow {
  getResultByName(name: string): unknown;
}

export interface QueryContext {
  searchString: string;
  trimmedSearchString: string;
  lowerCaseSearchString: string;
  sources: number[];
  tokens: Array<{ value: string }>;
  floorpWorkspaceBookmarkContext?: WorkspaceBookmarkContext;
}

export interface QueryResult {
  source: number;
  payload: { url?: string; title?: string };
}

export interface NativeProvider {
  queryInstance: object;
  startQuery(
    context: QueryContext,
    callback: (provider: NativeProvider, result: QueryResult) => void,
  ): Promise<void>;
  cancelQuery(): void;
}

export type AutofillQuery = [
  string | null,
  Record<string, string | number | null> | null,
];

export interface NativeAutofill extends NativeProvider {
  _searchString: string;
  _strippedPrefix: string;
  _getOriginQuery(context: QueryContext): AutofillQuery;
  _getUrlQuery(context: QueryContext): AutofillQuery;
  _getAdaptiveHistoryQuery(context: QueryContext): AutofillQuery;
}

export interface UrlbarModules {
  places: NativePlaces;
  scopes: WorkspaceBookmarksService;
  ProviderPlaces: new () => NativeProvider;
  ProviderKeywords: new () => NativeProvider;
  ProviderAutofill: {
    new (): NativeAutofill;
    getTopHostOverThreshold(
      context: QueryContext,
      hosts: string[],
    ): Promise<string | null>;
  };
  QueryContext: new (options: Record<string, unknown>) => QueryContext;
  tokenize(context: QueryContext): Array<{ value: string }>;
  sources: { HISTORY: number; BOOKMARKS: number };
}
