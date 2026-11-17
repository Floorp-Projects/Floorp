/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TWorkspaceID, TWorkspaceSnapshot } from "./type.ts";
import type { TWorkspaceSnapshotTab } from "./type.ts";
import { isUnassignedWorkspaceID } from "./workspaces-static-names.ts";
import {
  DEFAULT_WORKSPACE_BACKUP_CONFIG,
  WORKSPACE_BACKUP_KINDS,
  type WorkspaceBackupConfig,
  type WorkspaceBackupFile,
  type WorkspaceBackupKind,
  type WorkspaceBackupSummary,
} from "./backup-types.ts";

export const BACKUP_FILE_PREFIX = "backup-";
export const BACKUP_FILE_EXTENSION = ".jsonlz4";

const BACKUP_FILE_NAME_PATTERN =
  /^backup-(\d{8})-(\d{6})-([0-9a-f]{8})\.jsonlz4$/i;

export const isRecordValue = (
  value: unknown,
): value is Record<string, unknown> =>
  typeof value === "object" && value !== null && !Array.isArray(value);

export const isWorkspaceBackupKind = (
  value: unknown,
): value is WorkspaceBackupKind =>
  typeof value === "string" &&
  (WORKSPACE_BACKUP_KINDS as readonly string[]).includes(value);

const pad = (value: number, length = 2): string =>
  String(value).padStart(length, "0");

/**
 * Builds the human-sortable file name. Uses UTC so that file names sort
 * consistently regardless of the user's timezone; `capturedAt` inside the
 * file remains authoritative.
 */
export const formatBackupFileName = (
  capturedAt: number,
  shortId: string,
): string => {
  const date = new Date(capturedAt);
  const stamp = [
    date.getUTCFullYear(),
    pad(date.getUTCMonth() + 1),
    pad(date.getUTCDate()),
  ].join("") +
    "-" +
    [
      pad(date.getUTCHours()),
      pad(date.getUTCMinutes()),
      pad(date.getUTCSeconds()),
    ].join("");
  return `${BACKUP_FILE_PREFIX}${stamp}-${shortId}${BACKUP_FILE_EXTENSION}`;
};

export interface ParsedBackupFileName {
  fileName: string;
  shortId: string;
  timestamp: number;
}

export const parseBackupFileName = (
  fileName: string,
): ParsedBackupFileName | null => {
  const match = BACKUP_FILE_NAME_PATTERN.exec(fileName);
  if (!match) {
    return null;
  }
  const [, datePart, timePart, shortId] = match;
  const year = Number(datePart.slice(0, 4));
  const month = Number(datePart.slice(4, 6));
  const day = Number(datePart.slice(6, 8));
  const hour = Number(timePart.slice(0, 2));
  const minute = Number(timePart.slice(2, 4));
  const second = Number(timePart.slice(4, 6));
  const timestamp = Date.UTC(year, month - 1, day, hour, minute, second);
  if (!Number.isFinite(timestamp)) {
    return null;
  }
  return { fileName, shortId, timestamp };
};

export const getBaseName = (path: string): string => {
  const segments = path.split(/[/\\]/);
  return segments[segments.length - 1] ?? path;
};

export const isBackupFileName = (fileName: string): boolean =>
  BACKUP_FILE_NAME_PATTERN.test(fileName);

export const filterBackupFiles = (paths: string[]): string[] =>
  paths.filter((path) =>
    getBaseName(path).toLowerCase().endsWith(BACKUP_FILE_EXTENSION)
  );

export const isWorkspaceBackupFile = (
  value: unknown,
): value is WorkspaceBackupFile => {
  if (!isRecordValue(value)) {
    return false;
  }
  if (value.version !== 1) {
    return false;
  }
  if (!isWorkspaceBackupKind(value.kind)) {
    return false;
  }
  if (
    typeof value.capturedAt !== "number" || !Number.isFinite(value.capturedAt)
  ) {
    return false;
  }
  if (!Array.isArray(value.order)) {
    return false;
  }
  if (value.defaultID !== null && typeof value.defaultID !== "string") {
    return false;
  }
  if (
    value.currentWorkspaceId !== null &&
    typeof value.currentWorkspaceId !== "string"
  ) {
    return false;
  }
  return Array.isArray(value.workspaces);
};

export const buildBackupSummary = (
  fileName: string,
  file: WorkspaceBackupFile,
  fileSize: number,
): WorkspaceBackupSummary => {
  let tabCount = 0;
  let workspaceCount = 0;
  for (const workspace of file.workspaces) {
    // The virtual unassigned-workspace snapshot contributes its tabs, but it
    // is not a real workspace. Settings UI is implemented separately; it can
    // rely on workspaceCount being the number of real workspaces and
    // tabCount covering every tab stored in the file, including unassigned
    // tabs.
    tabCount += workspace.tabs.length;
    if (!isUnassignedWorkspaceID(workspace.workspace.workspaceId)) {
      workspaceCount++;
    }
  }
  return {
    id: fileName,
    kind: file.kind,
    capturedAt: file.capturedAt,
    workspaceCount,
    tabCount,
    fileName,
    fileSize,
  };
};

export const isUnassignedSnapshot = (
  snapshot: TWorkspaceSnapshot,
): boolean => isUnassignedWorkspaceID(snapshot.workspace.workspaceId);

/**
 * Chooses the tabs to reopen when restoring without touching the workspace
 * store (workspaces disabled). Tabs from the virtual unassigned snapshot are
 * preferred; if the backup has none, every snapshot's tabs are restored.
 */
export const collectTabsForWorkspacelessRestore = (
  file: WorkspaceBackupFile,
): TWorkspaceSnapshotTab[] => {
  const unassigned = file.workspaces.filter(isUnassignedSnapshot);
  const source = unassigned.length > 0 ? unassigned : file.workspaces;
  return source.flatMap((workspace) => workspace.tabs);
};

export const sortBackupsNewestFirst = (
  summaries: WorkspaceBackupSummary[],
): WorkspaceBackupSummary[] =>
  [...summaries].sort((a, b) => b.capturedAt - a.capturedAt);

/**
 * Age-based retention applies to automatic and manual backups only.
 * Pre-restore safety backups are exempt and capped separately.
 */
export const selectExpiredBackups = (
  summaries: WorkspaceBackupSummary[],
  retentionDays: number,
  now: number,
): WorkspaceBackupSummary[] => {
  if (!Number.isFinite(retentionDays) || retentionDays < 0) {
    return [];
  }
  const cutoff = now - retentionDays * 24 * 60 * 60 * 1000;
  return summaries.filter(
    (summary) => summary.kind !== "pre-restore" && summary.capturedAt < cutoff,
  );
};

/**
 * Keeps at most `maxPreRestoreBackups` most recent pre-restore backups and
 * returns the excess (oldest first).
 */
export const selectExcessPreRestoreBackups = (
  summaries: WorkspaceBackupSummary[],
  maxPreRestoreBackups: number,
): WorkspaceBackupSummary[] => {
  if (
    !Number.isFinite(maxPreRestoreBackups) || maxPreRestoreBackups < 0
  ) {
    return [];
  }
  const preRestore = summaries
    .filter((summary) => summary.kind === "pre-restore")
    .sort((a, b) => b.capturedAt - a.capturedAt);
  return preRestore.slice(maxPreRestoreBackups);
};

const toPositiveNumber = (value: unknown, fallback: number): number => {
  if (typeof value !== "number" || !Number.isFinite(value) || value <= 0) {
    return fallback;
  }
  return value;
};

const toNonNegativeNumber = (value: unknown, fallback: number): number => {
  if (typeof value !== "number" || !Number.isFinite(value) || value < 0) {
    return fallback;
  }
  return value;
};

/**
 * Decodes the backup config pref. Invalid or missing fields fall back to the
 * documented defaults instead of rejecting the whole object.
 */
export const normalizeBackupConfig = (
  raw: unknown,
): WorkspaceBackupConfig => {
  const source = isRecordValue(raw) ? raw : {};
  return {
    enabled: typeof source.enabled === "boolean"
      ? source.enabled
      : DEFAULT_WORKSPACE_BACKUP_CONFIG.enabled,
    intervalMinutes: toPositiveNumber(
      source.intervalMinutes,
      DEFAULT_WORKSPACE_BACKUP_CONFIG.intervalMinutes,
    ),
    retentionDays: toNonNegativeNumber(
      source.retentionDays,
      DEFAULT_WORKSPACE_BACKUP_CONFIG.retentionDays,
    ),
    maxPreRestoreBackups: toNonNegativeNumber(
      source.maxPreRestoreBackups,
      DEFAULT_WORKSPACE_BACKUP_CONFIG.maxPreRestoreBackups,
    ),
  };
};

export const serializeBackupConfig = (
  config: WorkspaceBackupConfig,
): string => JSON.stringify(config);

export const parseBackupConfig = (
  raw: string | null,
): WorkspaceBackupConfig => {
  if (!raw) {
    return { ...DEFAULT_WORKSPACE_BACKUP_CONFIG };
  }
  try {
    return normalizeBackupConfig(JSON.parse(raw));
  } catch (error) {
    console.error("WorkspacesBackup: failed to parse backup config", error);
    return { ...DEFAULT_WORKSPACE_BACKUP_CONFIG };
  }
};

export const buildBackupFile = (
  kind: WorkspaceBackupKind,
  capturedAt: number,
  order: TWorkspaceID[],
  defaultID: TWorkspaceID | null,
  currentWorkspaceId: TWorkspaceID | null,
  workspaces: TWorkspaceSnapshot[],
): WorkspaceBackupFile => ({
  version: 1,
  kind,
  capturedAt,
  order: [...order],
  defaultID,
  currentWorkspaceId,
  workspaces,
});
