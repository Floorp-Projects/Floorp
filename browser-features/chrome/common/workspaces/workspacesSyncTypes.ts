// SPDX-License-Identifier: MPL-2.0

import type {
  WorkspaceSyncDocument,
  WorkspaceSyncProvider,
  WorkspaceSyncSnapshot,
} from "#modules/common/workspace-sync-types.ts";

export interface IncomingWorkspaceNavigation {
  url: string;
  title: string;
  signal?: AbortSignal;
  promise: Promise<void>;
  finish(error?: Error): void;
  cancel(error: Error): void;
  checkStopped(progress: WorkspaceSyncProgress): void;
}

export interface WorkspaceSyncBrowser {
  currentURI?: { spec: string };
  browsingContext?: { originAttributes: { userContextId?: number } };
  stop(flags: number): void;
}

export interface WorkspaceSyncProgress {
  isTopLevel: boolean;
  isLoadingDocument: boolean;
  loadType: number;
}

export interface WorkspaceSyncProgressListener {
  onLocationChange(
    browser: WorkspaceSyncBrowser,
    progress: WorkspaceSyncProgress,
    request: nsIRequest | null,
    location: nsIURI,
    flags: number,
  ): void;
  onStateChange(
    browser: WorkspaceSyncBrowser,
    progress: WorkspaceSyncProgress,
    request: nsIRequest | null,
    flags: number,
    status: number,
  ): void;
}

export interface WorkspaceSyncTabBrowser {
  getBrowserForTab(tab: XULElement): WorkspaceSyncBrowser;
  addTabsProgressListener(listener: WorkspaceSyncProgressListener): void;
  removeTabsProgressListener(listener: WorkspaceSyncProgressListener): void;
  runBeforeUnloadForTabs(tabs: XULElement[]): Promise<boolean>;
  removeTab(
    tab: XULElement,
    options: {
      animate: boolean;
      skipPermitUnload?: boolean;
      skipSessionStore?: boolean;
    },
  ): void;
}

export interface WorkspaceContainerReopenState {
  token: string;
  workspaceId: string;
  tabId: string;
}

export interface WorkspaceSyncProgressFlags {
  STATE_START: number;
  STATE_STOP: number;
  STATE_IS_DOCUMENT: number;
  STATE_IS_NETWORK: number;
  LOCATION_CHANGE_ERROR_PAGE: number;
  LOCATION_CHANGE_SAME_DOCUMENT: number;
}

export interface SyncSessionStore {
  promiseAllWindowsRestored: Promise<void>;
  getCustomTabValue(tab: XULElement, key: string): string;
  setCustomTabValue(tab: XULElement, key: string, value: string): void;
  getTabState(tab: XULElement): string;
}

export interface WorkspaceSyncAPI {
  ready(): Promise<void>;
  isSelected(id: string): boolean;
  isRetired(id: string): boolean;
  errorFor(id: string): string | null;
  isPendingClose(id: string): boolean;
  noteTabClosed(workspaceId: string, tabId: string): Promise<void>;
  isContainerSyncEnabled(): boolean;
  isContainerApplyPending(id: string): boolean;
  isContainerDefaultPending(id: string): boolean;
  getSyncedWorkspace(id: string): WorkspaceSyncDocument | null;
  beginContainerReopen(workspaceId: string, tabId: string): Promise<string>;
  finishContainerReopen(token: string, success: boolean): Promise<void>;
  resolveLegacyContainers(
    workspaceId: string,
    target: number | "new",
    scope: "default" | "tabs" | "all",
  ): Promise<void>;
  getSyncedTab(
    workspaceId: string,
    tabId: string,
  ): WorkspaceSyncSnapshot["tabs"][number] | null;
  register(
    provider: WorkspaceSyncProvider,
    signal?: AbortSignal,
  ): Promise<() => void>;
  select(id: string, selected: boolean): Promise<void>;
  capture(): Promise<void>;
  deleteWorkspace(id: string): Promise<void>;
}
