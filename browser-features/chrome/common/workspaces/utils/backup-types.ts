/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TWorkspaceID, TWorkspaceSnapshot } from "./type.ts";

export type WorkspaceBackupKind = "auto" | "manual" | "pre-restore";

/**
 * Serialized workspace backup file (v1).
 *
 * `capturedAt` is authoritative; the timestamp embedded in the file name is
 * only for humans and for sorting.
 */
export interface WorkspaceBackupFile {
  version: 1;
  kind: WorkspaceBackupKind;
  capturedAt: number;
  order: TWorkspaceID[];
  defaultID: TWorkspaceID | null;
  currentWorkspaceId: TWorkspaceID | null;
  workspaces: TWorkspaceSnapshot[];
}

/**
 * Plain-serializable summary sent across the JSWindowActor bridge.
 */
export interface WorkspaceBackupSummary {
  /** Stable identifier used by the UI; currently the file name. */
  id: string;
  kind: WorkspaceBackupKind;
  capturedAt: number;
  workspaceCount: number;
  tabCount: number;
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
