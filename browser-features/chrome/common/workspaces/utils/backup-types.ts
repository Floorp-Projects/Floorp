/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TWorkspaceID } from "./type.ts";

export type WorkspaceBackupKind = "auto" | "manual" | "pre-restore";

/**
 * Capture mode of a backup file.
 *
 * - `workspaces`: captured while the workspaces feature was enabled. Tabs carry
 *   a `workspaceId` and the file carries `workspaceState`.
 * - `tabs`: captured while the workspaces feature was disabled. The file is a
 *   plain window/tab snapshot and never references the workspace store.
 */
export type WorkspaceBackupMode = "workspaces" | "tabs";

/**
 * Minimal shape needed to recreate a tab. Both backup tabs and archive
 * snapshot tabs satisfy this, so the restore primitive can be shared.
 */
export interface RestorableTab {
  state: Record<string, unknown> | null;
  title: string | null;
  url: string | null;
  pinned: boolean;
  isSelected: boolean;
  userContextId: number;
  lastShownWorkspaceId: TWorkspaceID | null;
}

/**
 * One tab inside a backup file.
 *
 * `workspaceId` is only present in `workspaces` mode and references
 * `WorkspaceBackupState.workspaces[].workspaceId`.
 */
export interface WorkspaceBackupTab extends RestorableTab {
  workspaceId?: TWorkspaceID;
}

export interface WorkspaceBackupWindow {
  /** True for the window that was most recently focused at capture time. */
  isFocused: boolean;
  tabs: WorkspaceBackupTab[];
}

/** Workspace metadata only; tabs are stored separately in windows[]. */
export interface WorkspaceBackupWorkspace {
  workspaceId: TWorkspaceID;
  name: string;
  userContextId: number;
  icon?: string | null;
}

/**
 * Workspace store snapshot. Present only in `workspaces` mode, so the
 * "disabled means no workspace data" invariant is structural.
 */
export interface WorkspaceBackupState {
  workspaces: WorkspaceBackupWorkspace[];
  order: TWorkspaceID[];
  defaultID: TWorkspaceID | null;
  /** Selection of the focused normal browser window when it can be read. */
  currentWorkspaceId: TWorkspaceID | null;
}

/**
 * Serialized workspace backup file (v2).
 *
 * `capturedAt` is authoritative; the timestamp embedded in the file name is
 * only for humans and for sorting.
 */
export interface WorkspaceBackupFile {
  version: 2;
  kind: WorkspaceBackupKind;
  mode: WorkspaceBackupMode;
  capturedAt: number;
  windows: WorkspaceBackupWindow[];
  /** Present only when `mode === "workspaces"`. */
  workspaceState?: WorkspaceBackupState;
}

/**
 * Plain-serializable summary sent across the JSWindowActor bridge.
 */
export interface WorkspaceBackupSummary {
  /** Stable identifier used by the UI; currently the file name. */
  id: string;
  kind: WorkspaceBackupKind;
  mode: WorkspaceBackupMode;
  capturedAt: number;
  windowCount: number;
  tabCount: number;
  /** Number of real workspaces. Always 0 in `tabs` mode. */
  workspaceCount: number;
  fileName: string;
  fileSize: number;
}

export interface WorkspaceBackupConfig {
  enabled: boolean;
  intervalMinutes: number;
  retentionDays: number;
  maxPreRestoreBackups: number;
}

export const DEFAULT_WORKSPACE_BACKUP_CONFIG: WorkspaceBackupConfig = {
  enabled: true,
  intervalMinutes: 15,
  retentionDays: 7,
  maxPreRestoreBackups: 10,
};

export const WORKSPACE_BACKUP_KINDS: readonly WorkspaceBackupKind[] = [
  "auto",
  "manual",
  "pre-restore",
];

export const WORKSPACE_BACKUP_MODES: readonly WorkspaceBackupMode[] = [
  "workspaces",
  "tabs",
];

/**
 * Hard limits applied while validating a backup file before it reaches any
 * destructive restore path. Values are deliberately generous: they exist to
 * reject corrupt or hostile files, not to constrain normal usage.
 */
export const WORKSPACE_BACKUP_LIMITS = {
  /** Maximum number of windows in one backup. */
  maxWindows: 20,
  /** Maximum number of tabs in one window. */
  maxTabsPerWindow: 1000,
  /** Maximum number of tabs across the whole backup. */
  maxTotalTabs: 5000,
  /** Maximum number of workspaces in one backup. */
  maxWorkspaces: 200,
  /** Maximum characters of a tab title kept in the file. */
  maxTitleLength: 4096,
  /** Maximum characters of a tab URL kept in the file. */
  maxUrlLength: 65536,
  /** Maximum bytes of one tab state, measured after serialization. */
  maxTabStateBytes: 1024 * 1024,
  /** Maximum characters of a workspace name. */
  maxNameLength: 256,
  /** Maximum characters of a workspace icon value. */
  maxIconLength: 2048,
  /** Maximum size of the decompressed JSON payload. */
  maxDecompressedBytes: 256 * 1024 * 1024,
  /** Maximum compressed file size accepted from disk. */
  maxFileBytes: 64 * 1024 * 1024,
} as const;
