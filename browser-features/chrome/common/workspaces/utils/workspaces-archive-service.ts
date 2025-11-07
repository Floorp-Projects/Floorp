/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TWorkspaceSnapshot } from "./type.ts";
import type {
  WorkspaceArchiveFile,
  WorkspaceArchiveSummary,
} from "./archive-types.ts";

const SNAPSHOT_FILE_EXTENSION = ".json";

const ensureDirectory = async (path: string) => {
  try {
    if (!(await IOUtils.exists(path))) {
      await IOUtils.makeDirectory(path);
    }
  } catch (error) {
    console.error("WorkspacesArchiveService: failed to ensure directory", {
      path,
      error,
    });
    throw error;
  }
};

const normalizeUUID = (uuid: string): string => uuid.replace(/[{}]/g, "");

const createArchiveId = () =>
  normalizeUUID(Services.uuid.generateUUID().toString());

const buildSummary = (
  archiveId: string,
  snapshot: TWorkspaceSnapshot,
  filePath: string,
): WorkspaceArchiveSummary => ({
  archiveId,
  workspaceId: snapshot.workspace.workspaceId,
  name: snapshot.workspace.name,
  icon: snapshot.workspace.icon ?? null,
  userContextId: snapshot.workspace.userContextId,
  capturedAt: snapshot.capturedAt,
  filePath,
});

const filterJsonFiles = (paths: string[]) =>
  paths.filter((path) => path.toLowerCase().endsWith(SNAPSHOT_FILE_EXTENSION));

const getArchiveDirectory = () =>
  PathUtils.join(PathUtils.profileDir, "workspaces", "archive");

const getSnapshotPath = (archiveId: string) =>
  PathUtils.join(
    getArchiveDirectory(),
    `${archiveId}${SNAPSHOT_FILE_EXTENSION}`,
  );

const safeReadJSON = async <T>(path: string): Promise<T | null> => {
  try {
    const json = await IOUtils.readJSON(path);
    return json as T;
  } catch (error) {
    console.error("WorkspacesArchiveService: failed to read JSON", {
      path,
      error,
    });
    return null;
  }
};

const safeWriteJSON = async (path: string, data: unknown) => {
  try {
    await IOUtils.writeJSON(path, data);
  } catch (error) {
    console.error("WorkspacesArchiveService: failed to write JSON", {
      path,
      error,
    });
    throw error;
  }
};

const safeRemoveFile = async (path: string) => {
  try {
    if (await IOUtils.exists(path)) {
      await IOUtils.remove(path);
    }
  } catch (error) {
    console.error("WorkspacesArchiveService: failed to remove file", {
      path,
      error,
    });
    throw error;
  }
};

const parseArchiveIdFromPath = (path: string): string => {
  const directory = getArchiveDirectory();
  let archiveId = path.startsWith(directory)
    ? path.substring(directory.length)
    : path;
  archiveId = archiveId.replace(/^[/\\]/, "");
  if (archiveId.toLowerCase().endsWith(SNAPSHOT_FILE_EXTENSION)) {
    archiveId = archiveId.slice(0, -SNAPSHOT_FILE_EXTENSION.length);
  }
  return archiveId;
};

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === "object" && value !== null;

const isWorkspaceArchiveFile = (
  value: unknown,
): value is WorkspaceArchiveFile => {
  if (!isRecord(value)) {
    return false;
  }
  if (value.version !== 1) {
    return false;
  }
  if (!("snapshot" in value) || !isRecord(value.snapshot)) {
    return false;
  }
  return true;
};

const decodeSnapshot = (data: unknown): TWorkspaceSnapshot => {
  if (!isWorkspaceArchiveFile(data)) {
    throw new Error("WorkspacesArchiveService: invalid archive file format");
  }
  return data.snapshot;
};

export class WorkspacesArchiveService {
  public async saveSnapshot(snapshot: TWorkspaceSnapshot): Promise<string> {
    const archiveDirectory = getArchiveDirectory();
    await ensureDirectory(archiveDirectory);

    const archiveId = createArchiveId();
    const path = getSnapshotPath(archiveId);

    await safeWriteJSON(path, {
      version: 1,
      snapshot,
    });

    return archiveId;
  }

  public async loadSnapshot(
    archiveId: string,
  ): Promise<TWorkspaceSnapshot | null> {
    const path = getSnapshotPath(archiveId);
    const data = await safeReadJSON<unknown>(path);
    if (!data) {
      return null;
    }
    try {
      return decodeSnapshot(data);
    } catch (error) {
      console.error("WorkspacesArchiveService: failed to decode snapshot", {
        archiveId,
        error,
      });
      return null;
    }
  }

  public async removeSnapshot(archiveId: string): Promise<void> {
    const path = getSnapshotPath(archiveId);
    await safeRemoveFile(path);
  }

  public async listSnapshots(): Promise<WorkspaceArchiveSummary[]> {
    const archiveDirectory = getArchiveDirectory();
    const exists = await IOUtils.exists(archiveDirectory);
    if (!exists) {
      return [];
    }

    const children = await IOUtils.getChildren(archiveDirectory);
    const summaries: WorkspaceArchiveSummary[] = [];

    for (const child of filterJsonFiles(children)) {
      const archiveId = parseArchiveIdFromPath(child);
      const data = await safeReadJSON<unknown>(child);
      if (!data) {
        continue;
      }
      try {
        const snapshot = decodeSnapshot(data);
        const summary = buildSummary(archiveId, snapshot, child);
        summaries.push(summary);
      } catch (error) {
        console.error("WorkspacesArchiveService: skipping invalid archive", {
          child,
          error,
        });
      }
    }

    return summaries.sort((a, b) => b.capturedAt - a.capturedAt);
  }

  public async clearAll(): Promise<void> {
    const archiveDirectory = getArchiveDirectory();
    const exists = await IOUtils.exists(archiveDirectory);
    if (!exists) {
      return;
    }

    const children = await IOUtils.getChildren(archiveDirectory);
    for (const child of filterJsonFiles(children)) {
      await safeRemoveFile(child);
    }
  }
}
