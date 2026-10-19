export interface NativeURLBrowser {
  contentTitle: string;
  messageManager: { loadFrameScript(url: string, delayed: boolean): void };
  browsingContext: {
    currentWindowGlobal?: { documentPrincipal: nsIPrincipal };
  };
}
export interface NativeURLTab {
  linkedBrowser: NativeURLBrowser;
}
export interface NativeURLWindow {
  gBrowser: {
    addTab(
      url: string,
      details: { triggeringPrincipal: nsIPrincipal },
    ): NativeURLTab;
    removeTab(tab: NativeURLTab): void;
  };
}
export interface NativeURLExtension {
  baseURI: nsIURI;
  backgroundContext?: { viewType?: string };
  policy: {
    backgroundServiceWorkerAllowed: boolean;
    backgroundTypeModule: boolean;
  };
}
export interface NativeURLResult {
  requestId: string;
  error?: string;
  value?: {
    id: string;
    sender?: { id?: string; url?: string; origin?: string };
    externalSender?: {
      id?: string;
      url?: string;
      origin?: string;
      receiverWorker?: boolean;
    };
    worker: boolean;
    urls: string[];
    detached: string;
    foreignThis: string;
    legacyType: string;
    legacy?: string;
    body: string;
    popup?: string;
    foreignPopupDenied?: boolean;
    credentialPopupDenied?: boolean;
    portPopupDenied?: boolean;
    offscreen?: boolean;
    foreignOffscreenDenied?: boolean;
    sidePanel?: { path?: string };
    foreignSidePanelDenied?: boolean;
    imported?: string;
    staticImported?: string;
  };
}
