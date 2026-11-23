// SPDX-License-Identifier: MPL-2.0

import type { WorkspaceContainerRef } from "./workspace-container-types.ts";

export interface WorkspaceSyncClock {
  modifiedAt: number;
  modifiedBy: string;
}

export interface WorkspaceSyncTab extends WorkspaceSyncClock {
  id: string;
  url: string;
  title: string;
  pinned: boolean;
  /** Required by the v2 parser; absent only in legacy v1 data. */
  container?: WorkspaceContainerRef;
  position: number;
  deleted: boolean;
}

export interface WorkspaceSyncDocument {
  version: 1 | 2;
  workspaceId: string;
  name: string;
  icon: string | null;
  /** Required by the v2 parser; absent only in legacy v1 data. */
  defaultContainer?: WorkspaceContainerRef;
  metadata: WorkspaceSyncClock;
  tabs: WorkspaceSyncTab[];
}

export interface WorkspaceSyncSnapshot {
  /** Explicit when beginning a v2 capture; otherwise inherits the previous document. */
  version?: 1 | 2;
  workspaceId: string;
  name: string;
  icon: string | null;
  defaultContainer?: WorkspaceContainerRef;
  orderChanged?: boolean;
  /** Missing protected tabs remain desired, rather than becoming outgoing tombstones. */
  protectedTabIds?: string[];
  tabs: Pick<
    WorkspaceSyncTab,
    "id" | "url" | "title" | "pinned" | "container"
  >[];
}

export interface WorkspaceSyncApplyResult {
  pendingContainerTabIds: string[];
  pendingDefaultContainer?: boolean;
}

export interface WorkspaceSyncProvider {
  window: Window;
  capture(
    workspaceId: string,
    seenIds: Set<string>,
  ): WorkspaceSyncSnapshot | null;
  apply(
    document: WorkspaceSyncDocument,
    missingTabIds: ReadonlySet<string>,
    /** Async providers must recheck cancellation before changing browser UI. */
    signal?: AbortSignal,
  ): Promise<void | WorkspaceSyncApplyResult>;
  tabIds(workspaceId: string): string[];
  containerTabIds?(userContextIds: readonly number[]): Record<string, string[]>;
}
