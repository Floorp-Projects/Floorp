// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />

/** Native surfaces used by the real capture and filesystem component suites. */
export interface ComponentBrowser {
  contentTitle: string;
  currentURI: nsIURI;
  browsingContext: {
    browserId: number;
    currentWindowGlobal: {
      documentPrincipal: nsIPrincipal;
      getActor(name: string): JSWindowActorParent;
    };
  };
  messageManager: {
    loadFrameScript(source: string, delayed: boolean): void;
  };
  fixupAndLoadURIString(
    uri: string,
    options: { triggeringPrincipal: nsIPrincipal },
  ): void;
  reload(): void;
}

export interface ComponentTab {
  linkedBrowser: ComponentBrowser;
  isConnected: boolean;
}

export interface ComponentNotification {
  secondaryActions: {
    callback(options: { checkboxChecked: boolean }): void;
  }[];
}

export interface ComponentNativeWindow extends Window {
  gBrowser: {
    selectedTab: ComponentTab;
    addTab(
      uri: string,
      options: { triggeringPrincipal: nsIPrincipal },
    ): ComponentTab;
    removeTab(tab: ComponentTab): void;
  };
  PopupNotifications: {
    getNotification(
      name: string,
      browser: ComponentBrowser | undefined,
    ): ComponentNotification | null;
    remove(notification: ComponentNotification): void;
  };
  wrappedJSObject: { __floorpRuntimeFixtureURL: string };
}

export interface ComponentAddon {
  uninstall(): Promise<void>;
}

export interface ComponentAddonManager {
  installTemporaryAddon(file: nsIFile): Promise<ComponentAddon>;
}

export interface ComponentContext {
  viewType: string;
  xulBrowser: ComponentBrowser;
  browsingContext: {
    currentWindowContext: { documentPrincipal: nsIPrincipal };
  };
  withCallContextData<T>(
    data: { isHandlingUserInput: boolean },
    run: () => T,
  ): T;
}

interface ComponentCaptureAPI {
  getCapturedTabs(): Promise<{ tabId: number; status: string }[]>;
  getMediaStreamId(): Promise<string>;
  _consumeStreamId(token: string): Promise<{ sessionId: string }>;
  _streamStopped(sessionId: string): Promise<void>;
}

interface ComponentAPIManager {
  asyncGetAPI(
    name: "browserAction",
    extension: ComponentExtension,
    scope: "addon_parent",
  ): Promise<{ action: { triggerClickOrPopup(tab: ComponentTab): void } }>;
  asyncGetAPI(
    name: "tabCapture",
    extension: ComponentExtension,
    scope: "addon_parent",
  ): Promise<{
    getAPI(context: ComponentContext): { tabCapture: ComponentCaptureAPI };
  }>;
  asyncGetAPI(
    name: "offscreen",
    extension: ComponentExtension,
    scope: "addon_parent",
  ): Promise<{
    getAPI(context: ComponentContext): {
      offscreen: {
        createDocument(options: {
          url: string;
          reasons: string[];
          justification: string;
        }): Promise<void>;
      };
    };
  }>;
}

export interface ComponentExtension {
  backgroundContext: ComponentContext | null;
  baseURI: nsIURI;
  principal: nsIPrincipal;
  views: Set<ComponentContext>;
  tabManager: { getWrapper(tab: ComponentTab): { id: number } };
  apiManager: ComponentAPIManager;
}

export interface ComponentExtensionParent {
  GlobalManager: { extensionMap: Map<string, ComponentExtension> };
}

export interface ActivationError {
  name: string;
  message: string;
}

export interface ConsumerRecording {
  bytes: number;
  pixel: number[];
  replayDenied: boolean;
  namespace: string;
}

export interface FileSystemResult {
  text: string;
  fetched: string;
  nativeFile: boolean;
  byteQuota: boolean;
  traversal: boolean;
  pixel: number[];
  usage: { used: number; quota: number };
  largeIntegrity: boolean;
  oversizedBuffer: boolean;
  malformedBuffer: boolean;
  url: string;
}

export interface FileSystemProbe {
  fetchDenied?: boolean;
  imageDenied?: boolean;
  downloadDenied?: boolean;
  globalsAbsent?: boolean;
  violations?: string[];
  entryQuota?: boolean;
  copyQuota?: boolean;
  pending?: boolean;
}
