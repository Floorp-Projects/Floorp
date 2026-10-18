// SPDX-License-Identifier: MPL-2.0

import type {
  WorkspaceSyncProvider,
  WorkspaceSyncSnapshot,
} from "#modules/common/workspace-sync-types.ts";

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
