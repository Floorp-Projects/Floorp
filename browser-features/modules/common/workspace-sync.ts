// SPDX-License-Identifier: MPL-2.0

import type {
  WorkspaceSyncClock,
  WorkspaceSyncDocument,
  WorkspaceSyncSnapshot,
  WorkspaceSyncTab,
} from "./workspace-sync-types.ts";

export const WORKSPACE_SYNC_COLLECTION = "floorp-workspaces";
export const WORKSPACE_SYNC_CHANGED = "floorp.workspaces.sync.changed";

export function isWorkspaceSyncId(value: unknown): value is string {
  return typeof value === "string" &&
    /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(
      value,
    );
}

export function isSyncableWorkspaceURL(value: unknown): value is string {
  if (
    typeof value !== "string" || value.length > 8192 || !value.isWellFormed()
  ) return false;
  try {
    const url = new URL(value);
    return (url.protocol === "https:" || url.protocol === "http:") &&
      !url.username && !url.password;
  } catch {
    return false;
  }
}

function object(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function clock(value: unknown): value is WorkspaceSyncClock {
  return object(value) && typeof value.modifiedAt === "number" &&
    Number.isSafeInteger(value.modifiedAt) && value.modifiedAt >= 0 &&
    isWorkspaceSyncId(value.modifiedBy);
}

/** Validate before opening URLs or saving an incoming, decrypted record. */
export function parseWorkspaceSyncDocument(
  value: unknown,
): WorkspaceSyncDocument {
  if (
    !object(value) || value.version !== 1 ||
    !isWorkspaceSyncId(value.workspaceId) ||
    typeof value.name !== "string" || value.name.length > 256 ||
    !value.name.isWellFormed() ||
    (value.icon !== null &&
      (typeof value.icon !== "string" || value.icon.length > 256 ||
        !value.icon.isWellFormed())) ||
    !clock(value.metadata) || !Array.isArray(value.tabs)
  ) throw new Error("Invalid workspace sync document");

  const ids = new Set<string>();
  const tabs: WorkspaceSyncTab[] = value.tabs.map((tab: unknown) => {
    if (
      !object(tab) || !clock(tab) || !isWorkspaceSyncId(tab.id) ||
      ids.has(tab.id) ||
      typeof tab.deleted !== "boolean" || typeof tab.pinned !== "boolean" ||
      typeof tab.title !== "string" || tab.title.length > 1024 ||
      !tab.title.isWellFormed() ||
      typeof tab.position !== "number" || !Number.isSafeInteger(tab.position) ||
      tab.position < 0 || !isSyncableWorkspaceURL(tab.url)
    ) throw new Error("Invalid workspace sync tab");
    ids.add(tab.id);
    return {
      id: tab.id,
      url: tab.url,
      title: tab.title,
      pinned: tab.pinned,
      position: tab.position,
      deleted: tab.deleted,
      modifiedAt: tab.modifiedAt,
      modifiedBy: tab.modifiedBy,
    };
  });
  const result: WorkspaceSyncDocument = {
    version: 1,
    workspaceId: value.workspaceId,
    name: value.name,
    icon: value.icon as string | null,
    metadata: {
      modifiedAt: value.metadata.modifiedAt,
      modifiedBy: value.metadata.modifiedBy,
    },
    tabs: tabs.sort((a, b) => a.id.localeCompare(b.id)),
  };
  return result;
}

function newest<T extends WorkspaceSyncClock>(a: T, b: T): T {
  if (a.modifiedAt !== b.modifiedAt) return a.modifiedAt > b.modifiedAt ? a : b;
  if (a.modifiedBy !== b.modifiedBy) return a.modifiedBy > b.modifiedBy ? a : b;
  // A canonical tie break makes merging commutative even for malformed clocks.
  return JSON.stringify(a) >= JSON.stringify(b) ? a : b;
}

function newestTab(a: WorkspaceSyncTab, b: WorkspaceSyncTab): WorkspaceSyncTab {
  if (a.deleted !== b.deleted) return a.deleted ? a : b;
  return newest(a, b);
}

/** A per-tab merge retains concurrent additions and persistent close markers. */
export function mergeWorkspaceSyncDocuments(
  local: WorkspaceSyncDocument,
  remote: WorkspaceSyncDocument,
): WorkspaceSyncDocument {
  if (local.workspaceId !== remote.workspaceId) {
    throw new Error("Workspace ID mismatch");
  }
  const metadata = newest(
    { ...local.metadata, name: local.name, icon: local.icon },
    { ...remote.metadata, name: remote.name, icon: remote.icon },
  );
  const tabs = new Map(local.tabs.map((tab) => [tab.id, tab]));
  for (const tab of remote.tabs) {
    const previous = tabs.get(tab.id);
    tabs.set(tab.id, previous ? newestTab(previous, tab) : tab);
  }
  return parseWorkspaceSyncDocument({
    version: 1,
    workspaceId: local.workspaceId,
    name: metadata.name,
    icon: metadata.icon,
    metadata: {
      modifiedAt: metadata.modifiedAt,
      modifiedBy: metadata.modifiedBy,
    },
    tabs: [...tabs.values()],
  });
}

export function updateWorkspaceSyncDocument(
  previous: WorkspaceSyncDocument | null,
  snapshot: WorkspaceSyncSnapshot,
  modifiedBy: string,
  now: number,
): WorkspaceSyncDocument {
  let lastClock = previous?.metadata.modifiedAt ?? 0;
  let nextPosition = 0;
  for (const tab of previous?.tabs ?? []) {
    lastClock = Math.max(lastClock, tab.modifiedAt);
    if (!tab.deleted) nextPosition = Math.max(nextPosition, tab.position + 1);
  }
  const modifiedAt = Math.max(now, lastClock + 1);
  const stamp = { modifiedAt, modifiedBy };
  const tabs = new Map(previous?.tabs.map((tab) => [tab.id, tab]));
  const present = new Set<string>();
  snapshot.tabs.filter((tab) => isSyncableWorkspaceURL(tab.url)).forEach(
    (tab, physicalPosition) => {
      if (present.has(tab.id)) {
        throw new Error("Duplicate local workspace tab ID");
      }
      present.add(tab.id);
      const old = tabs.get(tab.id);
      if (old?.deleted) return;
      const position = snapshot.orderChanged === false
        ? old?.position ?? nextPosition++
        : physicalPosition;
      if (
        !old || old.url !== tab.url || old.title !== tab.title ||
        old.pinned !== tab.pinned || old.position !== position
      ) {
        tabs.set(tab.id, { ...tab, position, deleted: false, ...stamp });
      }
    },
  );
  for (const [id, tab] of tabs) {
    if (!present.has(id) && !tab.deleted) {
      tabs.set(id, { ...tab, deleted: true, ...stamp });
    }
  }
  return parseWorkspaceSyncDocument({
    version: 1,
    workspaceId: snapshot.workspaceId,
    name: snapshot.name,
    icon: snapshot.icon,
    metadata: !previous || previous.name !== snapshot.name ||
        previous.icon !== snapshot.icon
      ? stamp
      : previous.metadata,
    tabs: [...tabs.values()],
  });
}

export function liveWorkspaceSyncTabs(
  document: WorkspaceSyncDocument,
): WorkspaceSyncTab[] {
  return document.tabs.filter((tab) => !tab.deleted).sort((a, b) =>
    a.position - b.position || a.id.localeCompare(b.id)
  );
}
