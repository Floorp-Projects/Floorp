/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TWorkspaceID } from "./type.ts";
import {
  DEFAULT_WORKSPACE_BACKUP_CONFIG,
  WORKSPACE_BACKUP_KINDS,
  WORKSPACE_BACKUP_LIMITS,
  WORKSPACE_BACKUP_MODES,
  type WorkspaceBackupConfig,
  type WorkspaceBackupFile,
  type WorkspaceBackupKind,
  type WorkspaceBackupMode,
  type WorkspaceBackupState,
  type WorkspaceBackupSummary,
  type WorkspaceBackupTab,
  type WorkspaceBackupWindow,
  type WorkspaceBackupWorkspace,
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

export const isWorkspaceBackupMode = (
  value: unknown,
): value is WorkspaceBackupMode =>
  typeof value === "string" &&
  (WORKSPACE_BACKUP_MODES as readonly string[]).includes(value);

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

/**
 * Returns the children that look like backup files. This is only a cheap
 * pre-filter; callers that read or delete must still validate the exact name
 * with `isBackupFileName`.
 */
export const filterBackupFiles = (paths: string[]): string[] =>
  paths.filter((path) =>
    getBaseName(path).toLowerCase().endsWith(BACKUP_FILE_EXTENSION)
  );

/* -------------------------------------------------------------------------
 * Tab state sanitization
 * ---------------------------------------------------------------------- */

/**
 * SessionStore keys that must never reach a backup file. Form data, session
 * storage and scroll position are the sensitive parts of a tab state and are
 * not needed to restore the tab itself.
 */
const SENSITIVE_STATE_KEYS: ReadonlySet<string> = new Set([
  "formdata",
  "storage",
  "scroll",
]);

/**
 * Floorp workspace markers live both at the top level of a tab state and
 * inside `attributes`. They are re-derived on restore, so they must not be
 * persisted as-is.
 */
const WORKSPACE_MARKER_KEYS: ReadonlySet<string> = new Set([
  "floorpWorkspaceId",
  "floorpLastShowWorkspaceId",
  "floorpDisableHistory",
]);

/**
 * Depth bound for the sanitizer. SessionStore tab states are shallow; the bound
 * only exists so a hostile file cannot make the walk pathological.
 */
const MAX_SANITIZE_DEPTH = 8;

const sanitizeValue = (value: unknown, depth: number): unknown => {
  if (depth > MAX_SANITIZE_DEPTH) {
    return null;
  }
  if (Array.isArray(value)) {
    return value.map((entry) => sanitizeValue(entry, depth + 1));
  }
  if (!isRecordValue(value)) {
    return value;
  }
  const copy: Record<string, unknown> = {};
  for (const [key, entry] of Object.entries(value)) {
    if (SENSITIVE_STATE_KEYS.has(key) || WORKSPACE_MARKER_KEYS.has(key)) {
      continue;
    }
    copy[key] = sanitizeValue(entry, depth + 1);
  }
  return copy;
};

/**
 * Removes sensitive and workspace-specific keys from a SessionStore tab state.
 *
 * The walk is recursive: `formdata` / `storage` / `scroll` are dropped at any
 * depth, so a nested occurrence cannot survive the sanitizer. Returns a new
 * object; the input is never mutated.
 */
export const sanitizeTabState = (
  state: Record<string, unknown> | null,
): Record<string, unknown> | null => {
  if (!state) {
    return null;
  }
  const result = sanitizeValue(state, 0);
  return isRecordValue(result) ? result : null;
};

/** True when the serialized state stays within the per-tab limit. */
export const isTabStateWithinLimit = (
  state: Record<string, unknown> | null,
): boolean => {
  if (!state) {
    return true;
  }
  try {
    return JSON.stringify(state).length <=
      WORKSPACE_BACKUP_LIMITS.maxTabStateBytes;
  } catch {
    return false;
  }
};

/* -------------------------------------------------------------------------
 * Runtime validation
 * ---------------------------------------------------------------------- */

const isFiniteNumber = (value: unknown): value is number =>
  typeof value === "number" && Number.isFinite(value);

const isSafeNonNegativeInteger = (value: unknown): value is number =>
  typeof value === "number" && Number.isSafeInteger(value) && value >= 0;

const isStringOrNull = (value: unknown): value is string | null =>
  value === null || typeof value === "string";

const isWorkspaceIdValue = (value: unknown): value is TWorkspaceID =>
  typeof value === "string" && value.length > 0;

export const isWorkspaceBackupTab = (
  value: unknown,
): value is WorkspaceBackupTab => {
  if (!isRecordValue(value)) {
    return false;
  }
  if (value.state !== null && !isRecordValue(value.state)) {
    return false;
  }
  if (!isStringOrNull(value.title) || !isStringOrNull(value.url)) {
    return false;
  }
  if (typeof value.pinned !== "boolean") {
    return false;
  }
  if (typeof value.isSelected !== "boolean") {
    return false;
  }
  if (!isSafeNonNegativeInteger(value.userContextId)) {
    return false;
  }
  if (
    value.lastShownWorkspaceId !== null &&
    !isWorkspaceIdValue(value.lastShownWorkspaceId)
  ) {
    return false;
  }
  if (
    value.workspaceId !== undefined && !isWorkspaceIdValue(value.workspaceId)
  ) {
    return false;
  }
  if (
    typeof value.title === "string" &&
    value.title.length > WORKSPACE_BACKUP_LIMITS.maxTitleLength
  ) {
    return false;
  }
  if (
    typeof value.url === "string" &&
    value.url.length > WORKSPACE_BACKUP_LIMITS.maxUrlLength
  ) {
    return false;
  }
  return isTabStateWithinLimit(value.state as Record<string, unknown> | null);
};

export const isWorkspaceBackupWindow = (
  value: unknown,
): value is WorkspaceBackupWindow => {
  if (!isRecordValue(value)) {
    return false;
  }
  if (typeof value.isFocused !== "boolean") {
    return false;
  }
  if (!Array.isArray(value.tabs)) {
    return false;
  }
  if (value.tabs.length > WORKSPACE_BACKUP_LIMITS.maxTabsPerWindow) {
    return false;
  }
  return value.tabs.every(isWorkspaceBackupTab);
};

export const isWorkspaceBackupWorkspace = (
  value: unknown,
): value is WorkspaceBackupWorkspace => {
  if (!isRecordValue(value)) {
    return false;
  }
  if (!isWorkspaceIdValue(value.workspaceId)) {
    return false;
  }
  if (typeof value.name !== "string") {
    return false;
  }
  if (!isSafeNonNegativeInteger(value.userContextId)) {
    return false;
  }
  if (
    value.icon !== undefined && value.icon !== null &&
    typeof value.icon !== "string"
  ) {
    return false;
  }
  return true;
};

export const isWorkspaceBackupState = (
  value: unknown,
): value is WorkspaceBackupState => {
  if (!isRecordValue(value)) {
    return false;
  }
  if (!Array.isArray(value.workspaces) || value.workspaces.length === 0) {
    return false;
  }
  if (value.workspaces.length > WORKSPACE_BACKUP_LIMITS.maxWorkspaces) {
    return false;
  }
  if (!value.workspaces.every(isWorkspaceBackupWorkspace)) {
    return false;
  }
  if (!Array.isArray(value.order)) {
    return false;
  }
  if (!value.order.every(isWorkspaceIdValue)) {
    return false;
  }
  if (value.defaultID !== null && !isWorkspaceIdValue(value.defaultID)) {
    return false;
  }
  if (
    value.currentWorkspaceId !== null &&
    !isWorkspaceIdValue(value.currentWorkspaceId)
  ) {
    return false;
  }
  return true;
};

/**
 * Validates a decoded backup file. Returns false for anything that is not a
 * well-formed v2 file; v1 files are rejected here by design (the v1 format was
 * never shipped).
 */
export const isWorkspaceBackupFile = (
  value: unknown,
): value is WorkspaceBackupFile => {
  if (!isRecordValue(value)) {
    return false;
  }
  if (value.version !== 2) {
    return false;
  }
  if (!isWorkspaceBackupKind(value.kind)) {
    return false;
  }
  if (!isWorkspaceBackupMode(value.mode)) {
    return false;
  }
  if (!isFiniteNumber(value.capturedAt)) {
    return false;
  }
  if (!Array.isArray(value.windows) || value.windows.length === 0) {
    return false;
  }
  if (value.windows.length > WORKSPACE_BACKUP_LIMITS.maxWindows) {
    return false;
  }
  if (!value.windows.every(isWorkspaceBackupWindow)) {
    return false;
  }
  let totalTabs = 0;
  for (const window of value.windows) {
    totalTabs += window.tabs.length;
  }
  if (totalTabs > WORKSPACE_BACKUP_LIMITS.maxTotalTabs) {
    return false;
  }
  if (value.mode === "workspaces") {
    return isWorkspaceBackupState(value.workspaceState);
  }
  // tabs mode must not carry workspace data at all.
  return value.workspaceState === undefined;
};

/**
 * Returns workspace IDs referenced by tabs but missing from the workspace
 * metadata. Such tabs are treated as unattributed on restore; the file itself
 * is not rejected.
 */
export const findDanglingWorkspaceReferences = (
  file: WorkspaceBackupFile,
): TWorkspaceID[] => {
  const state = file.workspaceState;
  if (!state) {
    return [];
  }
  const known = new Set(
    state.workspaces.map((workspace) => workspace.workspaceId),
  );
  const dangling: TWorkspaceID[] = [];
  for (const window of file.windows) {
    for (const tab of window.tabs) {
      if (tab.workspaceId && !known.has(tab.workspaceId)) {
        dangling.push(tab.workspaceId);
      }
    }
  }
  return dangling;
};

/* -------------------------------------------------------------------------
 * Summary / ordering / retention
 * ---------------------------------------------------------------------- */

export const buildBackupSummary = (
  fileName: string,
  file: WorkspaceBackupFile,
  fileSize: number,
): WorkspaceBackupSummary => {
  let tabCount = 0;
  for (const window of file.windows) {
    tabCount += window.tabs.length;
  }
  return {
    id: fileName,
    kind: file.kind,
    mode: file.mode,
    capturedAt: file.capturedAt,
    windowCount: file.windows.length,
    tabCount,
    workspaceCount: file.workspaceState?.workspaces.length ?? 0,
    fileName,
    fileSize,
  };
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

/* -------------------------------------------------------------------------
 * Config
 * ---------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------
 * Construction
 * ---------------------------------------------------------------------- */

export const buildBackupFile = (
  kind: WorkspaceBackupKind,
  mode: WorkspaceBackupMode,
  capturedAt: number,
  windows: WorkspaceBackupWindow[],
  workspaceState?: WorkspaceBackupState,
): WorkspaceBackupFile => {
  const file: WorkspaceBackupFile = {
    version: 2,
    kind,
    mode,
    capturedAt,
    windows,
  };
  if (mode === "workspaces" && workspaceState) {
    file.workspaceState = workspaceState;
  }
  return file;
};
