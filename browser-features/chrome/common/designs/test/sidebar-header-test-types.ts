// SPDX-License-Identifier: MPL-2.0

export type LitElement = HTMLElement & { updateComplete: Promise<boolean> };
export type SearchInput = LitElement & { inputEl: HTMLInputElement };
export type HistoryRow = HTMLElement & { mainEl: HTMLAnchorElement };
export type HistoryList = HTMLElement & {
  rowEls: NodeListOf<HistoryRow>;
  emptyState: Element | null;
};
export type HistoryPanel = LitElement & {
  controller: { searchQuery: string; sortOption: string };
  lists: NodeListOf<HistoryList>;
};
export type BookmarksPanel = LitElement & {
  searchQuery: string;
  searchResults: Array<{ guid: string; title: string }>;
};
export type SidebarTestController = {
  promiseInitialized: Promise<void>;
  browser: { contentDocument: Document };
  currentID: string;
  isOpen: boolean;
  sidebars: Map<string, { url: string }>;
  show(id: string): Promise<void>;
  hide(): void;
};
export type SidebarMenu = Element & {
  state: string;
  activateItem(item: Element): void;
  hidePopup(): void;
};
export type ExtensionBrowser = Element & {
  contentTitle: string;
  currentURI: { spec: string };
};
export type TemporaryAddon = { uninstall(): Promise<void> };
export type TestAddonManager = {
  installTemporaryAddon(file: nsIFile): Promise<TemporaryAddon>;
};
export type TestPlacesUtils = {
  bookmarks: {
    unfiledGuid: string;
    insert(info: {
      parentGuid: string;
      title: string;
      url: string;
    }): Promise<{ guid: string }>;
    remove(guid: string): Promise<void>;
  };
  history: {
    insert(info: {
      url: string;
      title: string;
      visits: Array<{ date: Date }>;
    }): Promise<unknown>;
    remove(url: string): Promise<unknown>;
  };
};
