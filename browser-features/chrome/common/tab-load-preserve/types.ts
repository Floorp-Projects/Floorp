// SPDX-License-Identifier: MPL-2.0

export interface PreserveLoadURI {
  readonly spec?: string;
}

export interface PreserveLoadChannel {
  readonly originalURI?: PreserveLoadURI | null;
  readonly URI?: PreserveLoadURI | null;
}

export interface PreserveLoadRequest {
  readonly originalURI?: PreserveLoadURI | null;
  readonly URI?: PreserveLoadURI | null;
  QueryInterface?: (iid: unknown) => PreserveLoadChannel;
}

export interface PreserveLoadWebProgress {
  readonly isTopLevel: boolean;
}

export interface PreserveLoadBrowser {
  userTypedValue: string | null;
  readonly currentURI?: PreserveLoadURI | null;
  readonly isNavigating?: boolean;
  didStartLoadSinceLastUserTyping?: () => boolean;
  fixupAndLoadURIString?: (
    uriString: string,
    params: {
      loadFlags: number;
      triggeringPrincipal?: unknown;
    },
  ) => void;
}

export interface PreserveLoadTab {
  readonly selected: boolean;
  readonly closing?: boolean;
  readonly linkedBrowser?: PreserveLoadBrowser | null;
}

export interface PreserveLoadTabContainer {
  addEventListener: (
    type: string,
    listener: EventListenerOrEventListenerObject,
  ) => void;
  removeEventListener: (
    type: string,
    listener: EventListenerOrEventListenerObject,
  ) => void;
}

export interface PreserveLoadProgressListener {
  onStateChange?: (
    browser: PreserveLoadBrowser,
    webProgress: PreserveLoadWebProgress,
    request: PreserveLoadRequest | null,
    stateFlags: number,
    status: number,
  ) => void;
  onLocationChange?: (
    browser: PreserveLoadBrowser,
    webProgress: PreserveLoadWebProgress,
    request: PreserveLoadRequest | null,
    location: PreserveLoadURI | null,
    flags: number,
  ) => void;
}

export interface PreserveLoadTabBrowser {
  readonly selectedTab: PreserveLoadTab | null;
  readonly selectedTabs?: readonly PreserveLoadTab[];
  readonly tabContainer: PreserveLoadTabContainer;
  getTabForBrowser: (browser: PreserveLoadBrowser) => PreserveLoadTab | null;
  addTabsProgressListener: (listener: PreserveLoadProgressListener) => void;
  removeTabsProgressListener: (listener: PreserveLoadProgressListener) => void;
  setInitialTabTitle?: (
    tab: PreserveLoadTab,
    title: string,
    options?: { isURL?: boolean },
  ) => void;
  reload?: () => void;
  reloadTab?: (tab: PreserveLoadTab) => void;
  reloadTabs?: (tabs: readonly PreserveLoadTab[]) => void;
  reloadWithFlags?: (reloadFlags: number) => void;
}

export interface PreserveLoadUrlbar {
  setURI: (options?: { dueToTabSwitch?: boolean }) => void;
}

export interface PreserveLoadDocument {
  getElementById: (id: string) => Element | null;
}

export interface TabActiveLoadState {
  readonly url: string;
  readonly fromUserTyped: boolean;
  readonly fromBlankOrInitial: boolean;
  committed: boolean;
}

export interface TabLoadRecord {
  activeLoad: TabActiveLoadState | null;
  preservedUrl: string | null;
}

export interface TabLoadPreserveControllerOptions {
  tabBrowser: PreserveLoadTabBrowser;
  urlbar: PreserveLoadUrlbar | null;
  document: PreserveLoadDocument | null;
  isBlankPageURL?: (url: string) => boolean;
  isInitialPage?: (url: string) => boolean;
  isSuccessCode?: (status: number) => boolean;
  getSystemPrincipal?: () => unknown;
  cancelledOldLoadStatus?: number;
}
