// SPDX-License-Identifier: MPL-2.0

import type {
  WorkspaceSyncClock,
  WorkspaceSyncTab,
} from "./workspace-sync-types.ts";

export interface WorkspaceSyncMetadataRecordValue {
  version: 1;
  kind: "workspace";
  workspaceId: string;
  name: string;
  icon: string | null;
  metadata: WorkspaceSyncClock;
}

export interface WorkspaceSyncTabRecordValue {
  version: 1;
  kind: "tab";
  workspaceId: string;
  tab: WorkspaceSyncTab & { deleted: false };
}

export type WorkspaceSyncRecordValue =
  | WorkspaceSyncMetadataRecordValue
  | WorkspaceSyncTabRecordValue;
