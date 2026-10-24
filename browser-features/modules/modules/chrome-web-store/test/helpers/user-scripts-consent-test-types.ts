// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />

export interface UserScriptsConsentStatus {
  eligible: boolean;
  allowed: boolean;
  permissionGranted: boolean;
}

export interface UserScriptsConsentService {
  getStatus(id: string):
    | UserScriptsConsentStatus
    | Promise<UserScriptsConsentStatus>;
}

export interface UserScriptsUIReply<Result> {
  data: { result?: Result; error?: string };
}

export interface UserScriptsUIBrowser {
  contentTitle: string;
  currentURI: nsIURI;
  browsingContext: {
    currentWindowGlobal: {
      innerWindowId: number;
      documentURI: nsIURI;
      getActor(name: string): {
        sendQuery<Result>(
          name: string,
          data: { id: string },
        ): Promise<Result>;
      };
    } | null;
  };
  messageManager: {
    loadFrameScript(source: string, delayed: boolean): void;
    addMessageListener<Result>(
      name: string,
      listener: (message: UserScriptsUIReply<Result>) => void,
    ): void;
    removeMessageListener<Result>(
      name: string,
      listener: (message: UserScriptsUIReply<Result>) => void,
    ): void;
  };
}

export interface UserScriptsUITab {
  linkedBrowser: UserScriptsUIBrowser;
  isConnected: boolean;
}

export interface UserScriptsUIWindow extends Window {
  wrappedJSObject: { __floorpRuntimeFixtureURL: string };
  gBrowser: {
    selectedTab: UserScriptsUITab;
    addTab(
      uri: string,
      options: { triggeringPrincipal: nsIPrincipal },
    ): UserScriptsUITab;
    removeTab(tab: UserScriptsUITab): void;
  };
}

export interface UserScriptsUIState {
  url: string;
  addonId: string | null;
  componentPresent: boolean;
  componentHidden: boolean;
  togglePresent: boolean;
  toggleVisible: boolean;
  pressed: boolean | null;
  disabled: boolean | null;
  label: string | null;
}

export interface UserScriptsTrustedInput {
  type: string;
  trusted: boolean;
}

export interface UserScriptsConsentUIEvidence {
  addonId: string;
  requestedAllowed: boolean;
  before: UserScriptsUIState;
  after: UserScriptsUIState;
  statusBefore: UserScriptsConsentStatus;
  statusAfter: UserScriptsConsentStatus;
  nativeEvents: UserScriptsTrustedInput[];
}

export interface UserScriptsConsentExtension {
  baseURI: nsIURI;
  backgroundContext: { viewType: string } | null;
}

export interface UserScriptsConsentParent {
  GlobalManager: {
    extensionMap: Map<string, UserScriptsConsentExtension>;
  };
}

export interface UserScriptsConsentProbe {
  generation: string;
  namespace: string;
  publicPermission: boolean;
  privatePermissionsHidden: boolean;
  internalRequestRejected: boolean;
  hasCaptured?: boolean;
  sameNamespace?: boolean;
  sameMethod?: boolean;
  currentRejected?: boolean;
  capturedRejected?: boolean;
  scripts?: { id: string }[];
}

export interface UserScriptsConsentCommandReply {
  requestId: string;
  result?: UserScriptsConsentProbe;
  error?: string;
}

export interface UserScriptsInjectionSnapshot {
  user: string | null;
  main: string | null;
  start: string | null;
}

export interface UserScriptsHeldDocumentSnapshot
  extends UserScriptsInjectionSnapshot {
  readyState: string;
}

export interface UserScriptsBarrierStatus {
  scriptRequested: boolean;
  released: boolean;
  timedOut: boolean;
}

export interface UserScriptsConsentActorMessage {
  name: string;
  data?: { id?: unknown };
}

export interface UserScriptsConsentResourceProtocol {
  setSubstitution(name: string, uri: nsIURI | null): void;
}

export type UserScriptsConsentUICommand =
  | "Initialized"
  | "Navigate"
  | "PermissionsVisible"
  | "ClickPermissions"
  | "PanelVisible"
  | "Snapshot"
  | "ClickToggle";

export interface UserScriptsConsentNativePage extends Window {
  promiseInitialized?: Promise<unknown>;
  gViewController?: { loadView(view: string): void | Promise<void> };
}

export interface UserScriptsConsentNativeToggle extends HTMLElement {
  pressed: boolean;
  disabled: boolean;
  buttonEl: HTMLButtonElement;
  updateComplete?: Promise<unknown>;
  label?: string;
  labelEl?: HTMLElement;
}
