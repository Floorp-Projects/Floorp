/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { onCleanup } from "solid-js";
import { selectedWorkspaceID, workspacesDataStore } from "../data/data.ts";
import {
  createUnassignedWorkspaceSnapshot,
  createWorkspaceSnapshotMetadata,
  ensureSessionStore,
  parseTabState,
  toWorkspaceSnapshotTab,
} from "./workspace-snapshot.ts";
import type {
  TWorkspace,
  TWorkspaceID,
  TWorkspaceSnapshot,
  TWorkspaceSnapshotTab,
} from "./type.ts";
import {
  WORKSPACE_BACKUP_CONFIG_PREF_NAME,
  WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
  WORKSPACE_ENABLED_PREF_NAME,
  WORKSPACE_TAB_ATTRIBUTION_ID,
} from "./workspaces-static-names.ts";
import {
  buildBackupFile,
  buildBackupSummary,
  filterBackupFiles,
  formatBackupFileName,
  getBaseName,
  isBackupFileName,
  isWorkspaceBackupFile,
  parseBackupConfig,
  selectExcessPreRestoreBackups,
  selectExpiredBackups,
  serializeBackupConfig,
  sortBackupsNewestFirst,
} from "./workspaces-backup-helpers.ts";
import type {
  WorkspaceBackupConfig,
  WorkspaceBackupFile,
  WorkspaceBackupKind,
  WorkspaceBackupSummary,
} from "./backup-types.ts";

interface BrowserWindow extends Window {
  closed: boolean;
  gBrowser?: {
    tabs: XULElement[];
    selectedTab?: XULElement;
  };
}

interface PrivateBrowsingUtilsModule {
  PrivateBrowsingUtils: {
    isWindowPrivate(window: Window): boolean;
  };
}

interface IOUtilsWithWriteAtomic {
  writeAtomic(
    path: string,
    data: Uint8Array,
    options?: WriteOptions,
  ): Promise<number>;
}

const ioUtils = IOUtils as unknown as typeof IOUtils & IOUtilsWithWriteAtomic;

const normalizeUUID = (uuid: string): string => uuid.replace(/[{}]/g, "");

const createShortId = (): string =>
  normalizeUUID(Services.uuid.generateUUID().toString()).slice(0, 8);

const getBackupDirectory = (): string =>
  PathUtils.join(PathUtils.profileDir, "workspaces", "backup");

const ensureDirectory = async (path: string): Promise<void> => {
  if (!(await IOUtils.exists(path))) {
    await IOUtils.makeDirectory(path, { createAncestors: true });
  }
};

const readLastRunAt = (): number => {
  const raw = Services.prefs.getStringPref(
    WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
    "0",
  );
  const parsed = Number.parseInt(raw, 10);
  return Number.isFinite(parsed) ? parsed : 0;
};

const writeLastRunAt = (value: number): void => {
  Services.prefs.setStringPref(
    WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
    String(value),
  );
};

const readBackupConfig = (): WorkspaceBackupConfig =>
  parseBackupConfig(
    Services.prefs.getStringPref(WORKSPACE_BACKUP_CONFIG_PREF_NAME, ""),
  );

const isPrivateWindow = (window: Window): boolean => {
  try {
    const module = ChromeUtils.importESModule(
      "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
    ) as unknown as PrivateBrowsingUtilsModule;
    return module.PrivateBrowsingUtils.isWindowPrivate(window);
  } catch (error) {
    console.error(
      "WorkspacesBackupService: failed to detect private window",
      error,
    );
    const privateUI = (
      window as unknown as {
        gPrivateBrowsingUI?: { privateWindow?: boolean };
      }
    ).gPrivateBrowsingUI;
    return privateUI?.privateWindow === true;
  }
};

/**
 * The subset of WorkspacesService behavior that backup capture/restore
 * needs. A detached implementation is used while the workspaces feature is
 * disabled and no WorkspacesService instance exists.
 */
export interface WorkspacesBackupHost {
  isWorkspaceID(id: string): id is TWorkspaceID;
  getRawWorkspace(id: TWorkspaceID): TWorkspace | undefined;
  restoreFromBackupFile(file: WorkspaceBackupFile): Promise<void>;
}

/**
 * Periodic workspace backup service.
 *
 * One instance exists per browser window. Scheduled runs are idempotent across
 * windows through the `floorp.workspaces.backup.lastRun` pref: every instance
 * checks the timestamp immediately before capturing. The timestamp is written
 * before capture, which means a crash during capture can delay the next run by
 * one interval; that trade-off is preferred over concurrent duplicate backups.
 */
export class WorkspacesBackupService {
  private readonly host: WorkspacesBackupHost;
  private config: WorkspaceBackupConfig;
  private timer: ReturnType<typeof globalThis.setTimeout> | null = null;
  private readonly configObserver: () => void;
  private readonly enabledObserver: () => void;
  private disposed = false;

  constructor(host: WorkspacesBackupHost) {
    this.host = host;
    this.config = readBackupConfig();
    this.configObserver = () => {
      this.config = readBackupConfig();
      this.cleanupRetention().catch((error) => {
        console.error(
          "WorkspacesBackupService: retention cleanup failed",
          error,
        );
      });
      this.scheduleNext(1_000);
    };
    this.enabledObserver = () => this.scheduleNext(1_000);
    Services.prefs.addObserver(
      WORKSPACE_BACKUP_CONFIG_PREF_NAME,
      this.configObserver,
    );
    Services.prefs.addObserver(
      WORKSPACE_ENABLED_PREF_NAME,
      this.enabledObserver,
    );
    onCleanup(() => this.dispose());

    this.cleanupRetention().catch((error) => {
      console.error(
        "WorkspacesBackupService: startup retention cleanup failed",
        error,
      );
    });
    this.scheduleNext(5_000);
  }

  public getConfig(): WorkspaceBackupConfig {
    return { ...this.config };
  }

  public updateConfig(config: WorkspaceBackupConfig): void {
    this.config = { ...config };
    Services.prefs.setStringPref(
      WORKSPACE_BACKUP_CONFIG_PREF_NAME,
      serializeBackupConfig(this.config),
    );
    this.scheduleNext(1_000);
  }

  public async backupNow(
    kind: WorkspaceBackupKind = "manual",
  ): Promise<WorkspaceBackupSummary> {
    if (kind !== "pre-restore") {
      // Write the run timestamp before capturing to reduce duplicate runs
      // across windows. See the class comment for the trade-off.
      writeLastRunAt(Date.now());
    }

    const file = await this.captureAllWorkspaces(kind);
    const directory = getBackupDirectory();
    await ensureDirectory(directory);

    const fileName = formatBackupFileName(file.capturedAt, createShortId());
    const path = PathUtils.join(directory, fileName);
    const bytes = new TextEncoder().encode(JSON.stringify(file));
    await ioUtils.writeAtomic(path, bytes, {
      compress: true,
      tmpPath: `${path}.tmp`,
    });

    const stat = await IOUtils.stat(path);
    await this.cleanupRetention();
    return buildBackupSummary(fileName, file, stat.size ?? 0);
  }

  public async listBackups(): Promise<WorkspaceBackupSummary[]> {
    const directory = getBackupDirectory();
    if (!(await IOUtils.exists(directory))) {
      return [];
    }

    const children = await IOUtils.getChildren(directory);
    const summaries: WorkspaceBackupSummary[] = [];
    for (const child of filterBackupFiles(children)) {
      const summary = await this.readSummary(child);
      if (summary) {
        summaries.push(summary);
      }
    }
    return sortBackupsNewestFirst(summaries);
  }

  public async deleteBackup(fileName: string): Promise<boolean> {
    if (!isBackupFileName(fileName)) {
      console.error(
        "WorkspacesBackupService: refusing to delete invalid backup name",
        fileName,
      );
      return false;
    }
    try {
      const path = PathUtils.join(getBackupDirectory(), fileName);
      if (await IOUtils.exists(path)) {
        await IOUtils.remove(path);
      }
      return true;
    } catch (error) {
      console.error("WorkspacesBackupService: failed to delete backup", error);
      return false;
    }
  }

  public async deleteAllBackups(): Promise<boolean> {
    try {
      const directory = getBackupDirectory();
      if (!(await IOUtils.exists(directory))) {
        return true;
      }
      const children = await IOUtils.getChildren(directory);
      for (const child of filterBackupFiles(children)) {
        await IOUtils.remove(child);
      }
      return true;
    } catch (error) {
      console.error(
        "WorkspacesBackupService: failed to delete all backups",
        error,
      );
      return false;
    }
  }

  public async restoreBackup(
    fileName: string,
  ): Promise<{ success: boolean; error?: string }> {
    if (!isBackupFileName(fileName)) {
      return { success: false, error: "invalid-file-name" };
    }
    const file = await this.loadBackup(fileName);
    if (!file) {
      return { success: false, error: "backup-not-found" };
    }

    try {
      await this.backupNow("pre-restore");
    } catch (error) {
      console.error(
        "WorkspacesBackupService: failed to capture pre-restore backup",
        error,
      );
      return { success: false, error: "pre-restore-failed" };
    }

    try {
      await this.host.restoreFromBackupFile(file);
      return { success: true };
    } catch (error) {
      console.error("WorkspacesBackupService: restore failed", error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  public dispose(): void {
    this.disposed = true;
    if (this.timer !== null) {
      globalThis.clearTimeout(this.timer);
      this.timer = null;
    }
    Services.prefs.removeObserver(
      WORKSPACE_BACKUP_CONFIG_PREF_NAME,
      this.configObserver,
    );
    Services.prefs.removeObserver(
      WORKSPACE_ENABLED_PREF_NAME,
      this.enabledObserver,
    );
  }

  public async cleanupRetention(): Promise<void> {
    const summaries = await this.listBackups();
    const now = Date.now();
    const expired = selectExpiredBackups(
      summaries,
      this.config.retentionDays,
      now,
    );
    const excessPreRestore = selectExcessPreRestoreBackups(
      summaries,
      this.config.maxPreRestoreBackups,
    );

    const toDelete = new Set<string>();
    for (const summary of [...expired, ...excessPreRestore]) {
      toDelete.add(summary.fileName);
    }
    for (const fileName of toDelete) {
      await this.deleteBackup(fileName);
    }
  }

  private async readSummary(
    path: string,
  ): Promise<WorkspaceBackupSummary | null> {
    const fileName = getBaseName(path);
    try {
      const file = await this.readBackupFile(path);
      if (!file) {
        return null;
      }
      const stat = await IOUtils.stat(path);
      return buildBackupSummary(fileName, file, stat.size ?? 0);
    } catch (error) {
      console.error(
        "WorkspacesBackupService: skipping unreadable backup",
        { path, error },
      );
      return null;
    }
  }

  private async readBackupFile(
    path: string,
  ): Promise<WorkspaceBackupFile | null> {
    const bytes = await IOUtils.read(path, { decompress: true });
    const text = new TextDecoder().decode(bytes);
    const parsed: unknown = JSON.parse(text);
    if (!isWorkspaceBackupFile(parsed)) {
      console.error(
        "WorkspacesBackupService: invalid backup file format",
        path,
      );
      return null;
    }
    return parsed;
  }

  private async loadBackup(
    fileName: string,
  ): Promise<WorkspaceBackupFile | null> {
    const path = PathUtils.join(getBackupDirectory(), fileName);
    if (!(await IOUtils.exists(path))) {
      return null;
    }
    try {
      return await this.readBackupFile(path);
    } catch (error) {
      console.error("WorkspacesBackupService: failed to load backup", error);
      return null;
    }
  }

  private getBrowserWindows(): BrowserWindow[] {
    const windows: BrowserWindow[] = [];
    const enumerator = Services.wm.getEnumerator("navigator:browser");
    while (enumerator.hasMoreElements()) {
      const window = enumerator.getNext() as BrowserWindow;
      if (!window || window.closed || isPrivateWindow(window)) {
        continue;
      }
      if (!window.gBrowser) {
        continue;
      }
      windows.push(window);
    }
    return windows;
  }

  private resolveCurrentWorkspaceId(
    focusedWindow: BrowserWindow | null,
    order: TWorkspaceID[],
  ): TWorkspaceID | null {
    const focusedTab = focusedWindow?.gBrowser?.selectedTab;
    const attributeId = focusedTab?.getAttribute(
      WORKSPACE_TAB_ATTRIBUTION_ID,
    );
    if (attributeId && order.includes(attributeId as TWorkspaceID)) {
      return attributeId as TWorkspaceID;
    }

    const selected = selectedWorkspaceID();
    if (selected && order.includes(selected)) {
      return selected;
    }
    return null;
  }

  private collectTabsForWorkspace(
    workspaceId: TWorkspaceID,
    windows: BrowserWindow[],
    focusedWindow: BrowserWindow | null,
    sessionStore: Awaited<ReturnType<typeof ensureSessionStore>>,
  ): TWorkspaceSnapshotTab[] {
    const tabs: TWorkspaceSnapshotTab[] = [];
    for (const window of windows) {
      const nodes = window.document.querySelectorAll(
        `[${WORKSPACE_TAB_ATTRIBUTION_ID}="${workspaceId}"]`,
      ) as NodeListOf<XULElement>;
      for (const tab of nodes) {
        const state = parseTabState(sessionStore, tab);
        const isSelected = window === focusedWindow &&
          tab === window.gBrowser?.selectedTab;
        tabs.push(toWorkspaceSnapshotTab(tab, state, this.host, isSelected));
      }
    }
    return tabs;
  }

  /**
   * Collects non-private tabs that have no workspace attribution. They are
   * stored in the virtual unassigned snapshot so backups cover every tab even
   * while the workspaces feature is disabled.
   */
  private collectTabsWithoutWorkspaceAttribute(
    windows: BrowserWindow[],
    focusedWindow: BrowserWindow | null,
    sessionStore: Awaited<ReturnType<typeof ensureSessionStore>>,
  ): TWorkspaceSnapshotTab[] {
    const tabs: TWorkspaceSnapshotTab[] = [];
    for (const window of windows) {
      const candidates = window.gBrowser?.tabs ?? [];
      for (const tab of candidates) {
        if (tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID)) {
          continue;
        }
        const state = parseTabState(sessionStore, tab);
        const isSelected = window === focusedWindow &&
          tab === window.gBrowser?.selectedTab;
        tabs.push(
          toWorkspaceSnapshotTab(tab, state, this.host, isSelected),
        );
      }
    }
    return tabs;
  }

  /**
   * Captures every workspace. Tabs are merged from all non-private browser
   * windows. Multi-window v1 limitation: tabs from different windows are
   * flattened into a single workspace snapshot, and only the most recently
   * focused window's selected tab is recorded as selected.
   */
  private async captureAllWorkspaces(
    kind: WorkspaceBackupKind,
  ): Promise<WorkspaceBackupFile> {
    const windows = this.getBrowserWindows();
    if (windows.length === 0) {
      throw new Error("no-browser-window");
    }
    const sessionStore = await ensureSessionStore();
    const focusedWindow = Services.wm.getMostRecentWindow(
      "navigator:browser",
    ) as BrowserWindow | null;
    const workspacesEnabled = Services.prefs.getBoolPref(
      WORKSPACE_ENABLED_PREF_NAME,
      true,
    );
    // While workspaces are disabled every tab is unassigned. Do not persist
    // stale workspace metadata from the previous enabled session into the
    // backup; the virtual unassigned snapshot below is the complete state.
    const order = workspacesEnabled
      ? [...workspacesDataStore.order] as TWorkspaceID[]
      : [];
    const workspaces: TWorkspaceSnapshot[] = [];

    if (workspacesEnabled) {
      for (const workspaceId of order) {
        const workspace = this.host.getRawWorkspace(workspaceId);
        if (!workspace) {
          continue;
        }
        workspaces.push({
          capturedAt: Date.now(),
          workspace: createWorkspaceSnapshotMetadata(workspaceId, workspace),
          tabs: this.collectTabsForWorkspace(
            workspaceId,
            windows,
            focusedWindow,
            sessionStore,
          ),
        });
      }
    }

    const unassignedTabs = this.collectTabsWithoutWorkspaceAttribute(
      windows,
      focusedWindow,
      sessionStore,
    );
    if (unassignedTabs.length > 0) {
      workspaces.push(createUnassignedWorkspaceSnapshot(unassignedTabs));
    }

    const defaultCandidate = workspacesDataStore.defaultID as TWorkspaceID;
    const defaultID = workspacesEnabled && order.includes(defaultCandidate)
      ? defaultCandidate
      : null;
    return buildBackupFile(
      kind,
      Date.now(),
      order,
      defaultID,
      workspacesEnabled
        ? this.resolveCurrentWorkspaceId(focusedWindow, order)
        : null,
      workspaces,
    );
  }

  private scheduleNext(delayMs?: number): void {
    if (this.timer !== null) {
      globalThis.clearTimeout(this.timer);
      this.timer = null;
    }
    if (this.disposed || !this.config.enabled) {
      return;
    }

    const intervalMs = Math.max(1, this.config.intervalMinutes) * 60_000;
    const elapsed = Date.now() - readLastRunAt();
    const delay = delayMs ?? Math.max(1_000, intervalMs - elapsed);
    this.timer = globalThis.setTimeout(() => {
      this.timer = null;
      this.runScheduledBackup()
        .catch((error) => {
          console.error(
            "WorkspacesBackupService: scheduled backup failed",
            error,
          );
        })
        .finally(() => this.scheduleNext());
    }, delay);
  }

  private async runScheduledBackup(): Promise<void> {
    // Workspaces may be disabled; unassigned tabs are still backed up via the
    // virtual snapshot. Only the backup feature's own enabled flag gates
    // scheduled runs.
    if (!this.config.enabled) {
      return;
    }
    const intervalMs = Math.max(1, this.config.intervalMinutes) * 60_000;
    if (Date.now() - readLastRunAt() < intervalMs) {
      return;
    }
    if (this.getBrowserWindows().length === 0) {
      return;
    }
    await this.backupNow("auto");
  }
}
