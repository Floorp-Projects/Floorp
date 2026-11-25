// SPDX-License-Identifier: MPL-2.0

import type {
  WorkspaceSyncClock,
  WorkspaceSyncTab,
} from "./workspace-sync-types.ts";
import type {
  WorkspaceContainerDefinition,
  WorkspaceContainerRef,
} from "./workspace-container-types.ts";

export interface WorkspaceSyncMetadataRecordValue {
  version: 1 | 2;
  kind: "workspace";
  workspaceId: string;
  name: string;
  icon: string | null;
  defaultContainer?: WorkspaceContainerRef;
  metadata: WorkspaceSyncClock;
}

export interface WorkspaceSyncTabRecordValue {
  version: 1 | 2;
  kind: "tab";
  workspaceId: string;
  tab: WorkspaceSyncTab & { deleted: false };
}

export type WorkspaceSyncRecordValue =
  | WorkspaceSyncMetadataRecordValue
  | WorkspaceSyncTabRecordValue
  | WorkspaceContainerDefinition;
