// SPDX-License-Identifier: MPL-2.0

import type {
  WorkspaceSyncProvider,
  WorkspaceSyncSnapshot,
} from "#modules/common/workspace-sync-types.ts";

export interface IncomingWorkspaceNavigation {
  url: string;
  title: string;
  historyIndex?: number;
  signal?: AbortSignal;
  promise: Promise<void>;
  finish(error?: Error): void;
  cancel(error: Error): void;
  checkStopped(progress: WorkspaceSyncProgress): void;
}

export interface WorkspaceSyncBrowser {
  currentURI?: { spec: string };
  browsingContext?: { sessionHistory?: { index: number } | null } | null;
  stop(flags: number): void;
}

export interface WorkspaceSyncProgress {
  isTopLevel: boolean;
  isLoadingDocument: boolean;
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
