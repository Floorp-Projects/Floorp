import type { WorkspaceBookmarksService } from "../WorkspaceBookmarks.sys.mts";

export interface BookmarkNode {
  bookmarkGuid: string;
  title: string;
  uri: string;
  type: number;
  parent: BookmarkNode | null;
  childCount: number;
  containerOpen: boolean;
  getChild(index: number): BookmarkNode;
}

export interface Query {
  parentCount: number;
  searchTerms: string;
  setParents(parents: string[]): void;
}

export interface Options {
  queryType: number;
  maxResults: number;
  sortingMode: number;
}

export interface BookmarkState {
  setWorkspaceWindow(window: Window): void;
  _titleChanged(value: string): Promise<void>;
  save(): Promise<string | undefined>;
}

export interface NativePlacesUI {
  workspaceBookmarkQuery(query: Query, options: Options, window: Window): {
    query: Query;
    options: Options;
    searchTerms: string;
    maxResults: number;
  };
  isWorkspaceBookmarkNodeVisible(node: BookmarkNode, window: Window): boolean;
  workspaceBookmarkMatches(node: BookmarkNode, terms: string): boolean;
  showBookmarkDialog(
    info: Record<string, unknown>,
    owner: Window,
  ): Promise<string | undefined>;
  promiseNodeLikeFromFetchInfo(
    info: Record<string, unknown>,
  ): Promise<BookmarkNode>;
  BookmarkState: new (options: {
    info: { itemGuid: string; parentGuid: string; title: string };
  }) => BookmarkState;
}

export interface PlacesFixture {
  bookmarks: {
    TYPE_FOLDER: number;
    unfiledGuid: string;
    unsavedGuid: string;
    insert(input: {
      parentGuid: string;
      type?: number;
      title: string;
      url?: string;
    }): Promise<{ guid: string; title: string; parentGuid: string }>;
    fetch(guid: string): Promise<{ guid: string; title: string } | null>;
    remove(guid: string): Promise<void>;
  };
  history: {
    getNewQuery(): Query;
    getNewQueryOptions(): Options;
    executeQuery(query: Query, options: Options): { root: BookmarkNode };
  };
}

export interface BookmarkMenu {
  _nativeView: boolean;
  _rebuildPopup(popup: Element): void;
  uninit(): void;
}

export interface BookmarkDialogWindow extends Window {
  gEditItemOverlay?: {
    bookmarkState?: BookmarkState | null;
    _floorpWorkspaceObserver?: unknown;
  };
}

export interface BrowserForBookmarks extends Window {
  gDialogBox: {
    isOpen: boolean;
    dialog:
      | { _frame: { contentWindow: BookmarkDialogWindow }; abort(): void }
      | null;
  };
  StarUI: {
    panel: XULPopupElement;
    selectedWorkspaceBookmarkGuid: string | null;
    _cancelOnPopupHidden: boolean;
    showEditBookmarkPopup(
      node: BookmarkNode,
      isNew: boolean,
      url: URL,
    ): Promise<void>;
    removeBookmarkButtonCommand(): void;
  };
  PlacesMenu: new (
    event: { target: Element; originalTarget: Element },
    query: string,
  ) => BookmarkMenu;
}

export interface BookmarkRuntime {
  scopes: WorkspaceBookmarksService;
  places: PlacesFixture;
  ui: NativePlacesUI;
}
