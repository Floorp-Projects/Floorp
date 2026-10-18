// SPDX-License-Identifier: MPL-2.0

export interface WorkspaceSyncClock {
  modifiedAt: number;
  modifiedBy: string;
}

export interface WorkspaceSyncTab extends WorkspaceSyncClock {
  id: string;
  url: string;
  title: string;
  pinned: boolean;
  position: number;
  deleted: boolean;
}

export interface WorkspaceSyncDocument {
  version: 1;
  workspaceId: string;
  name: string;
  icon: string | null;
  metadata: WorkspaceSyncClock;
  tabs: WorkspaceSyncTab[];
}

export interface WorkspaceSyncSnapshot {
  workspaceId: string;
  name: string;
  icon: string | null;
  orderChanged?: boolean;
  tabs: Pick<WorkspaceSyncTab, "id" | "url" | "title" | "pinned">[];
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
  ): Promise<void>;
  tabIds(workspaceId: string): string[];
}
