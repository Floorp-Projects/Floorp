// SPDX-License-Identifier: MPL-2.0

import {
  isWorkspaceSyncId,
  mergeWorkspaceSyncDocuments,
  migrateWorkspaceSyncDocumentToV2,
  parseWorkspaceSyncDocument,
} from "./workspace-sync.ts";
import {
  parseWorkspaceSyncRecord,
  workspaceContainerRecord,
  workspaceMetadataRecord,
  workspaceTabRecord,
} from "./workspace-sync-record.ts";
import type { WorkspaceSyncRecordValue } from "./workspace-sync-record-types.ts";
import type { WorkspaceSyncDocument } from "./workspace-sync-types.ts";
import type { WorkspaceContainerDefinition } from "./workspace-container-types.ts";

export interface WorkspaceMigrationRecord {
  id: string;
  deleted?: true;
  entity?: WorkspaceSyncRecordValue;
}

export interface WorkspaceMigrationInputRecord {
  id: string;
  deleted?: true;
  entity?: unknown;
}

export interface WorkspaceMigrationSnapshot {
  documents: WorkspaceSyncDocument[];
  metadataKnown: string[];
  definitions: WorkspaceContainerDefinition[];
  retiredIds: string[];
  records: WorkspaceMigrationRecord[];
}

/** Canonical cleartext is also the exact-write acknowledgement identity. */
export function canonicalWorkspaceMigrationRecord(
  record: WorkspaceMigrationInputRecord,
): string {
  if (!isWorkspaceSyncId(record.id)) {
    throw new Error("Invalid migration record ID");
  }
  if (record.deleted === true) {
    if (record.entity !== undefined) {
      throw new Error("Migration tombstone has entity");
    }
    return JSON.stringify({ id: record.id, deleted: true });
  }
  const entity = parseWorkspaceSyncRecord(record.entity, 2);
  const id = entity.kind === "container"
    ? entity.containerId
    : entity.kind === "workspace"
    ? entity.workspaceId
    : entity.tab.id;
  if (id !== record.id) throw new Error("Migration record identity mismatch");
  return JSON.stringify({ id, entity });
}

/**
 * Import the complete legacy collection, including orphan tabs and tombstones.
 * Selection, exclusions and other UI state are deliberately not in this value.
 */
export function buildWorkspaceMigrationSnapshot(
  legacy: readonly WorkspaceMigrationInputRecord[],
  localDocuments: readonly WorkspaceSyncDocument[],
  localRetired: readonly string[],
  definitions: readonly WorkspaceContainerDefinition[],
  localMetadataKnown?: readonly string[],
): WorkspaceMigrationSnapshot {
  const retired = new Set(localRetired);
  for (const id of retired) {
    if (!isWorkspaceSyncId(id)) throw new Error("Invalid retired migration ID");
  }
  const local = new Map<string, WorkspaceSyncDocument>();
  for (const raw of localDocuments) {
    const doc = migrateWorkspaceSyncDocumentToV2(raw);
    if (local.has(doc.workspaceId)) {
      throw new Error("Duplicate local migration workspace");
    }
    local.set(doc.workspaceId, doc);
    for (const tab of doc.tabs) if (tab.deleted) retired.add(tab.id);
  }
  const remote = new Map<string, WorkspaceSyncDocument>();
  const known = new Set(localMetadataKnown ?? local.keys());
  const seen = new Set<string>();
  const owners = new Map<string, string>();
  for (const record of legacy) {
    if (!isWorkspaceSyncId(record.id) || seen.has(record.id)) {
      throw new Error("Invalid or duplicate legacy migration record");
    }
    seen.add(record.id);
    if (record.deleted) {
      if (record.entity !== undefined) {
        throw new Error("Legacy tombstone has entity");
      }
      retired.add(record.id);
      continue;
    }
    const entity = parseWorkspaceSyncRecord(record.entity, 1);
    if (entity.kind === "container") {
      throw new Error("Container in legacy collection");
    }
    const id = entity.kind === "workspace" ? entity.workspaceId : entity.tab.id;
    if (id !== record.id) {
      throw new Error("Legacy migration record identity mismatch");
    }
    let doc = remote.get(entity.workspaceId);
    if (entity.kind === "workspace") {
      doc = {
        version: 1,
        workspaceId: entity.workspaceId,
        name: entity.name,
        icon: entity.icon,
        metadata: entity.metadata,
        tabs: doc?.tabs ?? [],
      };
      known.add(entity.workspaceId);
    } else {
      if (
        owners.has(entity.tab.id) &&
        owners.get(entity.tab.id) !== entity.workspaceId
      ) {
        throw new Error("Legacy tab changed workspace ownership");
      }
      owners.set(entity.tab.id, entity.workspaceId);
      doc ??= {
        version: 1,
        workspaceId: entity.workspaceId,
        name: "",
        icon: null,
        metadata: { modifiedAt: 0, modifiedBy: entity.tab.modifiedBy },
        tabs: [],
      };
      doc.tabs.push(entity.tab);
    }
    remote.set(entity.workspaceId, doc);
  }
  const merged = new Map(local);
  for (const [id, source] of remote) {
    const observed = local.get(id);
    const migrated = migrateWorkspaceSyncDocumentToV2(
      source,
      observed && {
        defaultContainer: observed.defaultContainer,
        tabs: Object.fromEntries(
          observed.tabs.flatMap((tab) =>
            tab.container ? [[tab.id, tab.container]] : []
          ),
        ),
      },
    );
    merged.set(
      id,
      observed ? mergeWorkspaceSyncDocuments(observed, migrated) : migrated,
    );
  }
  const documents: WorkspaceSyncDocument[] = [];
  for (const [id, raw] of merged) {
    if (retired.has(id)) continue;
    const doc = parseWorkspaceSyncDocument({
      ...raw,
      tabs: raw.tabs.filter((tab) => !retired.has(tab.id) && !tab.deleted),
    }, 2);
    for (const tab of doc.tabs) {
      const previousOwner = owners.get(tab.id);
      if (previousOwner && previousOwner !== id) {
        throw new Error("Migration tab ownership conflict");
      }
      if (merged.has(tab.id)) {
        throw new Error("Migration workspace/tab ID collision");
      }
      owners.set(tab.id, id);
    }
    documents.push(doc);
  }
  const all = new Map<string, WorkspaceMigrationRecord>();
  const add = (record: WorkspaceMigrationRecord) => {
    canonicalWorkspaceMigrationRecord(record);
    if (all.has(record.id)) throw new Error("Migration entity ID collision");
    all.set(record.id, record);
  };
  const canonicalDefinitions = definitions.map(workspaceContainerRecord);
  for (const definition of canonicalDefinitions) {
    add({ id: definition.containerId, entity: definition });
  }
  for (const doc of documents) {
    if (known.has(doc.workspaceId)) {
      add({ id: doc.workspaceId, entity: workspaceMetadataRecord(doc) });
    }
    for (const tab of doc.tabs) {
      add({ id: tab.id, entity: workspaceTabRecord(doc.workspaceId, tab, 2) });
    }
  }
  for (const id of retired) {
    add({ id, deleted: true });
  }
  return {
    documents: documents.sort((a, b) =>
      a.workspaceId.localeCompare(b.workspaceId)
    ),
    metadataKnown: [...known].filter((id) => !retired.has(id)).sort(),
    definitions: canonicalDefinitions,
    retiredIds: [...retired].sort(),
    records: [...all.values()].sort((a, b) => a.id.localeCompare(b.id)),
  };
}

/** Only an accepted upload of the staged bytes may advance migration. */
export function acknowledgeWorkspaceMigrationRecords(
  pending: readonly string[],
  staged: readonly WorkspaceMigrationRecord[],
  sent: readonly WorkspaceMigrationRecord[],
): string[] {
  const expected = new Map(
    staged.map((
      record,
    ) => [record.id, canonicalWorkspaceMigrationRecord(record)]),
  );
  const acknowledged = new Set(
    sent.filter((record) =>
      expected.get(record.id) === canonicalWorkspaceMigrationRecord(record)
    ).map((record) => record.id),
  );
  return pending.filter((id) => !acknowledged.has(id));
}
