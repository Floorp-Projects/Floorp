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
  // Date.UTC silently normalizes out-of-range fields (month 13, hour 25, years
  // 0-99). Re-format and compare so only canonical timestamps are accepted.
  const canonical = formatBackupFileName(timestamp, shortId);
  const start = BACKUP_FILE_PREFIX.length;
  if (canonical.slice(start, start + 15) !== `${datePart}-${timePart}`) {
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
 * Maximum timestamp accepted for `capturedAt` (end of year 9999 UTC). Keeps
 * file names (4-digit year) and `Date` arithmetic well defined.
 */
export const MAX_CAPTURED_AT = Date.UTC(9999, 11, 31);

/** The newest N auto/manual backups are never expired by age. */
export const MIN_KEEP_BACKUPS = 3;

const ALLOWED_ABOUT_PAGES: ReadonlySet<string> = new Set([
  "blank",
  "newtab",
  "home",
  "reader",
]);

/**
 * True for URLs that may be re-opened from a backup file: http, https,
 * moz-extension and a few harmless about: pages (blank, newtab, home, reader
 * wrapping an http(s) URL). Everything else (javascript:, data:, chrome:,
 * resource:, file:, view-source:, about:config, ...) is refused so a tampered
 * file cannot reach privileged pages.
 */
export const isRestorableTabUrl = (url: string): boolean => {
  if (typeof url !== "string" || url.length === 0) {
    return false;
  }
  let parsed: URL;
  try {
    parsed = new URL(url);
  } catch {
    return false;
  }
  switch (parsed.protocol) {
    case "http:":
    case "https:":
    case "moz-extension:":
      return true;
    case "about:": {
      const page = parsed.pathname.toLowerCase();
      if (!ALLOWED_ABOUT_PAGES.has(page)) {
        return false;
      }
      if (page === "reader") {
        // Reader view wraps another URL; that URL must be restorable itself.
        const inner = parsed.searchParams.get("url");
        return inner !== null &&
          (inner.startsWith("http://") || inner.startsWith("https://")) &&
          isRestorableTabUrl(inner);
      }
      return true;
    }
    default:
      return false;
  }
};

/**
 * Floorp workspace markers live both at the top level of a tab state and
 * inside `attributes`. They are re-derived on restore, so they must not be
 * persisted as-is.
 */
const WORKSPACE_MARKER_KEYS: ReadonlySet<string> = new Set([
  "floorpWorkspaceId",
  "floorpWorkspaceLastShowId",
  "floorpLastShowWorkspaceId",
  "floorpDisableHistory",
]);

/**
 * Depth bound for `children` nesting. SessionStore frames are shallow; the
 * bound only keeps a hostile file from making the walk pathological.
 */
const MAX_SANITIZE_DEPTH = 8;

const isNonNegativeSafeInt = (value: unknown): value is number =>
  typeof value === "number" && Number.isSafeInteger(value) && value >= 0;

/**
 * Allowlist of session history entry fields. Everything else (postdata_b64,
 * referrerInfo, triggeringPrincipal_base64, principalToInherit_base64,
 * partitionedPrincipalToInherit_base64, csp, structuredCloneState, formdata,
 * scroll, presState, ...) is dropped: it is either sensitive or an
 * attacker-controlled security principal.
 */
const sanitizeEntry = (
  entry: unknown,
  depth: number,
): Record<string, unknown> | null => {
  if (depth > MAX_SANITIZE_DEPTH || !isRecordValue(entry)) {
    return null;
  }
  if (typeof entry.url !== "string" || !isRestorableTabUrl(entry.url)) {
    return null;
  }
  const result: Record<string, unknown> = { url: entry.url };
  for (const key of ["title", "charset", "docshellUUID"] as const) {
    if (typeof entry[key] === "string") {
      result[key] = entry[key];
    }
  }
  if (typeof entry.ID === "string" || isNonNegativeSafeInt(entry.ID)) {
    result.ID = entry.ID;
  }
  for (const key of ["originalURI", "resultPrincipalURI"] as const) {
    const candidate = entry[key];
    if (typeof candidate === "string" && isRestorableTabUrl(candidate)) {
      result[key] = candidate;
    }
  }
  for (const key of ["hasUserInteraction", "persist"] as const) {
    if (typeof entry[key] === "boolean") {
      result[key] = entry[key];
    }
  }
  if (isNonNegativeSafeInt(entry.cacheKey)) {
    result.cacheKey = entry.cacheKey;
  }
  if (Array.isArray(entry.children)) {
    const children = entry.children
      .map((child) => sanitizeEntry(child, depth + 1))
      .filter((child): child is Record<string, unknown> => child !== null);
    if (children.length > 0) {
      result.children = children;
    }
  }
  return result;
};

/** Maps a 1-based history position onto the surviving entries. */
const remapIndex = (
  position: unknown,
  keptOriginalIndexes: number[],
): number | undefined => {
  if (!isNonNegativeSafeInt(position) || keptOriginalIndexes.length === 0) {
    return undefined;
  }
  const target = Math.max(position - 1, 0);
  let mapped = 0;
  for (let i = 0; i < keptOriginalIndexes.length; i++) {
    if (keptOriginalIndexes[i] <= target) {
      mapped = i;
    }
  }
  return mapped + 1;
};

/**
 * Reduces a SessionStore tab state to an allowlist (null when nothing
 * restorable remains).
 *
 * Top-level keys kept: `entries`, `index`, `requestedIndex`, `userContextId`,
 * `pinned`, `hidden`, `lastAccessed`, `image` (http/https only) and
 * `attributes` (primitive values, workspace markers removed). These are what
 * `SessionStore.setTabState` needs to rebuild the tab and its history.
 * Deliberately NOT kept: `userTypedValue`/`userTypedClear` (typed text),
 * `formdata`, `storage`, `scroll`, `extData`, and any unknown key.
 *
 * Entries (and `children`) keep only url, title, charset, ID, docshellUUID,
 * originalURI, resultPrincipalURI, hasUserInteraction, persist, cacheKey and
 * children. Entries whose URL fails `isRestorableTabUrl` are dropped and
 * `index`/`requestedIndex` are remapped. Returns a new object; the input is
 * never mutated.
 */
export const sanitizeTabState = (
  state: Record<string, unknown> | null,
): Record<string, unknown> | null => {
  if (!isRecordValue(state)) {
    return null;
  }
  const result: Record<string, unknown> = {};

  if (Array.isArray(state.entries)) {
    const kept: Record<string, unknown>[] = [];
    const keptOriginalIndexes: number[] = [];
    state.entries.forEach((entry, originalIndex) => {
      const sanitized = sanitizeEntry(entry, 0);
      if (sanitized) {
        kept.push(sanitized);
        keptOriginalIndexes.push(originalIndex);
      }
    });
    if (kept.length === 0) {
      return null;
    }
    result.entries = kept;
    result.index = remapIndex(state.index, keptOriginalIndexes) ?? kept.length;
    const requested = remapIndex(state.requestedIndex, keptOriginalIndexes);
    if (requested !== undefined) {
      result.requestedIndex = requested;
    }
  }

  if (isNonNegativeSafeInt(state.userContextId)) {
    result.userContextId = state.userContextId;
  }
  for (const key of ["pinned", "hidden"] as const) {
    if (typeof state[key] === "boolean") {
      result[key] = state[key];
    }
  }
  if (isNonNegativeSafeInt(state.lastAccessed)) {
    result.lastAccessed = state.lastAccessed;
  }
  if (
    typeof state.image === "string" &&
    (state.image.startsWith("http://") || state.image.startsWith("https://")) &&
    isRestorableTabUrl(state.image)
  ) {
    result.image = state.image;
  }
  if (isRecordValue(state.attributes)) {
    const attributes: Record<string, unknown> = {};
    for (const [key, value] of Object.entries(state.attributes)) {
      if (WORKSPACE_MARKER_KEYS.has(key)) {
        continue;
      }
      if (
        typeof value === "string" || typeof value === "boolean" ||
        typeof value === "number"
      ) {
        attributes[key] = value;
      }
    }
    result.attributes = attributes;
  }
  return result;
};

/** True when the serialized state stays within the per-tab limit. */
export const isTabStateWithinLimit = (
  state: Record<string, unknown> | null,
): boolean => {
  if (!state) {
    return true;
  }
  try {
    // Measure bytes, not UTF-16 code units: the limit is a byte budget.
    return new TextEncoder().encode(JSON.stringify(state)).length <=
      WORKSPACE_BACKUP_LIMITS.maxTabStateBytes;
  } catch {
    return false;
  }
};

/* -------------------------------------------------------------------------
 * Runtime validation
 * ---------------------------------------------------------------------- */

const isSafeNonNegativeInteger = (value: unknown): value is number =>
  typeof value === "number" && Number.isSafeInteger(value) && value >= 0;

const isStringOrNull = (value: unknown): value is string | null =>
  value === null || typeof value === "string";

// Same shape as zWorkspaceID in type.ts.
const WORKSPACE_ID_PATTERN =
  /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

const isWorkspaceIdValue = (value: unknown): value is TWorkspaceID =>
  typeof value === "string" && WORKSPACE_ID_PATTERN.test(value);

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
  if (
    typeof value.name !== "string" ||
    value.name.length > WORKSPACE_BACKUP_LIMITS.maxNameLength
  ) {
    return false;
  }
  if (!isSafeNonNegativeInteger(value.userContextId)) {
    return false;
  }
  if (
    value.icon !== undefined && value.icon !== null &&
    (typeof value.icon !== "string" ||
      value.icon.length > WORKSPACE_BACKUP_LIMITS.maxIconLength)
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
  if (
    value.order.length > WORKSPACE_BACKUP_LIMITS.maxWorkspaces ||
    !value.order.every(isWorkspaceIdValue)
  ) {
    return false;
  }
  // Referential integrity: unique ids, order is a permutation of them, and
  // defaultID / currentWorkspaceId (when set) point at real workspaces.
  const ids = new Set<string>();
  for (const workspace of value.workspaces as WorkspaceBackupWorkspace[]) {
    if (ids.has(workspace.workspaceId)) {
      return false;
    }
    ids.add(workspace.workspaceId);
  }
  const orderIds = value.order as string[];
  if (
    new Set(orderIds).size !== orderIds.length ||
    orderIds.length !== ids.size ||
    !orderIds.every((id) => ids.has(id))
  ) {
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
  if (typeof value.defaultID === "string" && !ids.has(value.defaultID)) {
    return false;
  }
  if (
    typeof value.currentWorkspaceId === "string" &&
    !ids.has(value.currentWorkspaceId)
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
  if (
    !isSafeNonNegativeInteger(value.capturedAt) ||
    value.capturedAt > MAX_CAPTURED_AT
  ) {
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
  // tabs mode must not carry workspace data at all, including on tabs.
  if (value.workspaceState !== undefined) {
    return false;
  }
  return value.windows.every((window) =>
    window.tabs.every((tab) => tab.workspaceId === undefined)
  );
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

/** Newest first; file name (descending) breaks ties deterministically. */
const compareNewestFirst = (
  a: WorkspaceBackupSummary,
  b: WorkspaceBackupSummary,
): number =>
  b.capturedAt - a.capturedAt ||
  (a.fileName < b.fileName ? 1 : a.fileName > b.fileName ? -1 : 0);

export const sortBackupsNewestFirst = (
  summaries: WorkspaceBackupSummary[],
): WorkspaceBackupSummary[] => [...summaries].sort(compareNewestFirst);

/**
 * Age-based retention applies to automatic and manual backups only.
 * Pre-restore safety backups are exempt and capped separately. The newest
 * `minKeep` auto/manual backups are never returned, however old they are.
 */
export const selectExpiredBackups = (
  summaries: WorkspaceBackupSummary[],
  retentionDays: number,
  now: number,
  minKeep: number = MIN_KEEP_BACKUPS,
): WorkspaceBackupSummary[] => {
  if (!Number.isFinite(retentionDays) || retentionDays < 0) {
    return [];
  }
  const keep = Number.isFinite(minKeep) && minKeep >= 0
    ? Math.floor(minKeep)
    : MIN_KEEP_BACKUPS;
  const cutoff = now - retentionDays * 24 * 60 * 60 * 1000;
  return summaries
    .filter((summary) => summary.kind !== "pre-restore")
    .sort(compareNewestFirst)
    .slice(keep)
    .filter((summary) => summary.capturedAt < cutoff);
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
  const keep = Math.floor(maxPreRestoreBackups);
  const preRestore = summaries
    .filter((summary) => summary.kind === "pre-restore")
    .sort(compareNewestFirst);
  // Keep the newest N and return the excess oldest-first, as documented.
  return preRestore.slice(keep).reverse();
};

/* -------------------------------------------------------------------------
 * Config
 * ---------------------------------------------------------------------- */

/**
 * Whole number clamped into [min, max]. Non-numbers and non-finite values
 * (NaN, +-Infinity) fall back to the default; fractions are floored.
 */
const toClampedInteger = (
  value: unknown,
  fallback: number,
  min: number,
  max: number,
): number => {
  if (typeof value !== "number" || !Number.isFinite(value)) {
    return fallback;
  }
  return Math.min(max, Math.max(min, Math.floor(value)));
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
    intervalMinutes: toClampedInteger(
      source.intervalMinutes,
      DEFAULT_WORKSPACE_BACKUP_CONFIG.intervalMinutes,
      1,
      1440,
    ),
    retentionDays: toClampedInteger(
      source.retentionDays,
      DEFAULT_WORKSPACE_BACKUP_CONFIG.retentionDays,
      1,
      365,
    ),
    maxPreRestoreBackups: toClampedInteger(
      source.maxPreRestoreBackups,
      DEFAULT_WORKSPACE_BACKUP_CONFIG.maxPreRestoreBackups,
      1,
      50,
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
