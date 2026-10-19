// SPDX-License-Identifier: MPL-2.0

import { parseWorkspaceSyncDocument } from "./workspace-sync.ts";
import type {
  WorkspaceSyncDocument,
  WorkspaceSyncTab,
} from "./workspace-sync-types.ts";
import type {
  WorkspaceSyncMetadataRecordValue,
  WorkspaceSyncRecordValue,
  WorkspaceSyncTabRecordValue,
} from "./workspace-sync-record-types.ts";

export const WORKSPACE_SYNC_RECORD_MAX_BYTES = 16 * 1024;

export function parseWorkspaceSyncRecord(
  value: unknown,
): WorkspaceSyncRecordValue {
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    throw new Error("Invalid workspace sync record");
  }
  const raw = value as Record<string, unknown>;
  if (raw.version !== 1) throw new Error("Unsupported workspace sync record");
  let result: WorkspaceSyncRecordValue;
  if (raw.kind === "workspace") {
    const document = parseWorkspaceSyncDocument({ ...raw, tabs: [] });
    result = {
      version: 1,
      kind: "workspace",
      workspaceId: document.workspaceId,
      name: document.name,
      icon: document.icon,
      metadata: document.metadata,
    };
  } else if (raw.kind === "tab") {
    const tab = raw.tab as Partial<WorkspaceSyncTab> | null | undefined;
    if (tab?.deleted !== false) {
      throw new Error("Tab deletion requires a Sync tombstone");
    }
    const document = parseWorkspaceSyncDocument({
      version: 1,
      workspaceId: raw.workspaceId,
      name: "",
      icon: null,
      metadata: {
        modifiedAt: tab.modifiedAt,
        modifiedBy: tab.modifiedBy,
      },
      tabs: [tab],
    });
    if (document.tabs[0].id === document.workspaceId) {
      throw new Error("Workspace and tab IDs must be distinct");
    }
    result = {
      version: 1,
      kind: "tab",
      workspaceId: document.workspaceId,
      tab: document.tabs[0] as WorkspaceSyncTab & { deleted: false },
    };
  } else {
    throw new Error("Invalid workspace sync record kind");
  }
  if (
    new TextEncoder().encode(JSON.stringify(result)).length >
      WORKSPACE_SYNC_RECORD_MAX_BYTES
  ) {
    throw new Error("Workspace sync record exceeds the payload limit");
  }
  return result;
}

export function workspaceMetadataRecord(
  document: WorkspaceSyncDocument,
): WorkspaceSyncMetadataRecordValue {
  return parseWorkspaceSyncRecord({
    version: 1,
    kind: "workspace",
    workspaceId: document.workspaceId,
    name: document.name,
    icon: document.icon,
    metadata: document.metadata,
  }) as WorkspaceSyncMetadataRecordValue;
}

export function workspaceTabRecord(
  workspaceId: string,
  tab: WorkspaceSyncTab,
): WorkspaceSyncTabRecordValue {
  return parseWorkspaceSyncRecord({
    version: 1,
    kind: "tab",
    workspaceId,
    tab,
  }) as WorkspaceSyncTabRecordValue;
}
