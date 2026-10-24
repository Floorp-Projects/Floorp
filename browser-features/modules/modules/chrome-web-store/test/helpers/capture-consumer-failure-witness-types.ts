// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />

export interface CaptureFailureBrowser {
  browserId: number;
  contentTitle: string;
  currentURI: nsIURI;
  remoteType: string;
  getBoundingClientRect(): DOMRect;
  browsingContext: {
    id: number;
    browserId: number;
    currentWindowGlobal: {
      innerWindowId: number;
      documentURI: nsIURI;
      documentPrincipal: nsIPrincipal;
    } | null;
  };
}

export interface CaptureFailureTab extends Element {
  hidden: boolean;
  selected: boolean;
  linkedPanel: string | null;
  linkedBrowser: CaptureFailureBrowser;
}

export interface CaptureFailureWindow extends Window {
  windowState: number;
  gBrowser: {
    selectedTab: CaptureFailureTab;
    visibleTabs: CaptureFailureTab[];
  };
}

export interface CaptureFailureTracker {
  orderedWindows: CaptureFailureWindow[];
  getAllVisibleTabs(): { browserId: number; contentTitle: string }[];
}
