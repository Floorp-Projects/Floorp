/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { onCleanup } from "solid-js";
import { selectedWorkspaceID, workspacesDataStore } from "../data/data.ts";
import {
  ensureSessionStore,
  parseTabState,
  type RestoreTabOutcome,
  restoreTabsIntoWindow,
  type RestoreTargetBrowser,
  toNumber,
  toWorkspaceSnapshotTab,
} from "./workspace-snapshot.ts";
import type { TWorkspace, TWorkspaceID } from "./type.ts";
import {
  WORKSPACE_BACKUP_CONFIG_PREF_NAME,
  WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
  WORKSPACE_ENABLED_PREF_NAME,
  WORKSPACE_TAB_ATTRIBUTION_ID,
} from "./workspaces-static-names.ts";
import { PrivateContainer } from "../../private-container/PrivateContainer.ts";
import {
  buildBackupFile,
  buildBackupSummary,
  formatBackupFileName,
  getBaseName,
  isBackupFileName,
  isWorkspaceBackupFile,
  parseBackupConfig,
  sanitizeTabState,
  selectExcessPreRestoreBackups,
  selectExpiredBackups,
  serializeBackupConfig,
  sortBackupsNewestFirst,
} from "./workspaces-backup-helpers.ts";
import {
  WORKSPACE_BACKUP_LIMITS,
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

interface BrowserWindow extends Window {
  closed: boolean;
  gBrowser?: RestoreTargetBrowser;
}

interface PrivateBrowsingUtilsModule {
  PrivateBrowsingUtils: {
    isWindowPrivate(window: Window): boolean;
  };
}

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

const isWorkspacesEnabled = (): boolean =>
  Services.prefs.getBoolPref(WORKSPACE_ENABLED_PREF_NAME, true);

/* -------------------------------------------------------------------------
 * Privacy
 * ---------------------------------------------------------------------- */

type PrivacyVerdict = "private" | "normal" | "unknown";

/**
 * Classifies a window. Unknown results are never treated as normal: callers
 * must exclude them so a failed privacy check can never leak private tabs into
 * a persistent backup.
 */
const getPrivacyVerdict = (window: Window): PrivacyVerdict => {
  try {
    const module = ChromeUtils.importESModule(
      "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
    ) as unknown as PrivateBrowsingUtilsModule;
    return module.PrivateBrowsingUtils.isWindowPrivate(window)
      ? "private"
      : "normal";
  } catch (error) {
    console.error(
      "WorkspacesBackupService: private window detection failed",
      error,
    );
    return "unknown";
  }
};

/**
 * Returns the userContextId of Floorp's private container, or null when the
 * feature is unavailable. Tabs in this container live in normal windows, so
 * window-level privacy checks cannot exclude them.
 */
const getPrivateContainerUserContextId = (): number | null => {
  try {
    return PrivateContainer.getPrivateContainerUserContextId();
  } catch (error) {
    console.error(
      "WorkspacesBackupService: private container lookup failed",
      error,
    );
    return null;
  }
};

/* -------------------------------------------------------------------------
 * Window helpers
 * ---------------------------------------------------------------------- */

interface DelayedStartupWindow extends Window {
  delayedStartupPromise?: Promise<void>;
  gBrowserInit?: {
    delayedStartupPromise?: Promise<void>;
    delayedStartupFinished?: unknown;
  };
}

const WINDOW_STARTUP_TIMEOUT_MS = 15_000;

/**
 * Waits until the target window finished its delayed startup. `readyState`
 * alone is not enough: gBrowser and SessionStore are wired during delayed
 * startup, and touching them earlier races the window's own restore.
 */
const waitForDelayedStartup = (window: DelayedStartupWindow): Promise<void> => {
  return new Promise<void>((resolve, reject) => {
    let settled = false;
    let observer: ((subject: nsISupports | null) => void) | null = null;
    let timer: ReturnType<typeof globalThis.setTimeout> | null = null;

    const cleanup = (): void => {
      if (timer !== null) {
        globalThis.clearTimeout(timer);
        timer = null;
      }
      if (observer) {
        try {
          Services.obs.removeObserver(
            observer,
            "browser-delayed-startup-finished",
          );
        } catch (error) {
          console.error(
            "WorkspacesBackupService: failed to remove startup observer",
            error,
          );
        }
        observer = null;
      }
    };
    const settle = (error?: unknown): void => {
      if (settled) {
        return;
      }
      settled = true;
      cleanup();
      if (error) {
        reject(error);
      } else {
        resolve();
      }
    };

    const startup = window.delayedStartupPromise ??
      window.gBrowserInit?.delayedStartupPromise;
    if (startup) {
      startup.then(() => settle()).catch((error) => settle(error));
      return;
    }
    if (window.gBrowserInit?.delayedStartupFinished) {
      settle();
      return;
    }

    observer = (subject: nsISupports | null) => {
      if ((subject as unknown) !== window) {
        return;
      }
      settle();
    };
    Services.obs.addObserver(observer, "browser-delayed-startup-finished");
    timer = globalThis.setTimeout(() => {
      settle(new Error("window-startup-timeout"));
    }, WINDOW_STARTUP_TIMEOUT_MS);

    window.addEventListener(
      "unload",
      () => settle(new Error("window-closed")),
      { once: true },
    );
  });
};

interface OpenBrowserWindowHost {
  OpenBrowserWindow(options?: Record<string, unknown>): BrowserWindow;
}

/**
 * Opens a new non-private browser window and waits for it to be ready.
 * `OpenBrowserWindow()` does not inherit private mode unless asked, so the new
 * window is always a normal one.
 */
const openNewBrowserWindow = async (): Promise<BrowserWindow> => {
  const host = globalThis as unknown as OpenBrowserWindowHost;
  if (typeof host.OpenBrowserWindow !== "function") {
    throw new Error("open-window-unavailable");
  }
  const window = host.OpenBrowserWindow({});
  await waitForDelayedStartup(window as unknown as DelayedStartupWindow);
  return window;
};

/* -------------------------------------------------------------------------
 * Service
 * ---------------------------------------------------------------------- */

/**
 * The subset of WorkspacesService behavior that backup restore needs. A
 * detached implementation is used while the workspaces feature is disabled and
 * no WorkspacesService instance exists.
 */
export interface WorkspacesBackupHost {
  isWorkspaceID(id: string): id is TWorkspaceID;
  getRawWorkspace(id: TWorkspaceID): TWorkspace | undefined;
  /** Replaces the whole workspace store from a validated backup file. */
  restoreWorkspaceState(file: WorkspaceBackupFile): Promise<void>;
}

/** Options for restoring a backup as plain tabs. */
export interface RestoreTabsOptions {
  /** Recreate the captured window layout instead of flattening into one window. */
  splitWindows: boolean;
  /** Workspace assigned to every restored tab, or null to leave them unassigned. */
  assignWorkspaceId: TWorkspaceID | null;
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
  private cleanupInFlight: Promise<void> | null = null;

  constructor(host: WorkspacesBackupHost) {
    this.host = host;
    this.config = readBackupConfig();
    this.configObserver = () => {
      const wasEnabled = this.config.enabled;
      this.config = readBackupConfig();
      if (!wasEnabled && this.config.enabled) {
        // Re-enabling should not wait for a stale interval.
        writeLastRunAt(0);
      }
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
    const wasEnabled = this.config.enabled;
    this.config = { ...config };
    Services.prefs.setStringPref(
      WORKSPACE_BACKUP_CONFIG_PREF_NAME,
      serializeBackupConfig(this.config),
    );
    if (!wasEnabled && this.config.enabled) {
      writeLastRunAt(0);
    }
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
    await writeBackupFile(path, bytes);

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
    for (const child of children) {
      const fileName = getBaseName(child);
      // Strict name validation for every file operation, not just delete.
      if (!isBackupFileName(fileName)) {
        continue;
      }
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
      for (const child of children) {
        if (!isBackupFileName(getBaseName(child))) {
          continue;
        }
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

  /**
   * Restores a backup file.
   *
   * Restore semantics follow the *current* workspaces setting, not the file
   * mode: the same button always does the same thing in a given configuration.
   * The file mode only decides which data is available.
   */
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

    const workspacesEnabled = isWorkspacesEnabled();
    const destructive = workspacesEnabled && file.mode === "workspaces";

    // A safety copy is only needed when the restore replaces existing state.
    if (destructive) {
      try {
        await this.backupNow("pre-restore");
      } catch (error) {
        console.error(
          "WorkspacesBackupService: failed to capture pre-restore backup",
          error,
        );
        return { success: false, error: "pre-restore-failed" };
      }
    }

    try {
      if (destructive) {
        await this.host.restoreWorkspaceState(file);
      } else if (workspacesEnabled) {
        // tabs file while workspaces are enabled: add them to the current
        // workspace, flattened into the focused window.
        await this.restoreTabs(file, {
          splitWindows: false,
          assignWorkspaceId: this.resolveCurrentWorkspaceId(),
        });
      } else {
        // Workspaces disabled: never touch the store. Keep the captured
        // window layout when the file has one.
        await this.restoreTabs(file, {
          splitWindows: file.mode === "tabs",
          assignWorkspaceId: null,
        });
      }
      return { success: true };
    } catch (error) {
      console.error("WorkspacesBackupService: restore failed", error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /** Restores a backup as plain tabs (no workspace store changes). */
  public async restoreTabs(
    file: WorkspaceBackupFile,
    options: RestoreTabsOptions,
  ): Promise<RestoreTabOutcome[]> {
    const currentWindow = this.getRestoreTargetWindow();
    const currentBrowser = currentWindow?.gBrowser;
    if (!currentWindow || !currentBrowser) {
      throw new Error("no-browser-window");
    }

    const windows = file.windows;
    const outcomes: RestoreTabOutcome[] = [];

    if (!options.splitWindows || windows.length <= 1) {
      const tabs = windows.flatMap((window) => window.tabs);
      return restoreTabsIntoWindow(tabs, {
        gBrowser: currentBrowser,
        resolveWorkspaceId: () => options.assignWorkspaceId,
      });
    }

    // Multi-window restore: the focused captured window lands in the current
    // window, every other captured window becomes a new window. Existing tabs
    // are never closed.
    const focusedIndex = windows.findIndex((window) => window.isFocused);
    const order: number[] = [];
    if (focusedIndex >= 0) {
      order.push(focusedIndex);
    }
    for (let index = 0; index < windows.length; index++) {
      if (index !== focusedIndex) {
        order.push(index);
      }
    }

    for (let position = 0; position < order.length; position++) {
      const captured = windows[order[position]];
      if (position === 0) {
        outcomes.push(
          ...await restoreTabsIntoWindow(captured.tabs, {
            gBrowser: currentBrowser,
            resolveWorkspaceId: () => options.assignWorkspaceId,
          }),
        );
        continue;
      }

      let newWindow: BrowserWindow;
      try {
        newWindow = await openNewBrowserWindow();
      } catch (error) {
        console.error(
          "WorkspacesBackupService: failed to open a restore window",
          error,
        );
        continue;
      }
      const newBrowser = newWindow.gBrowser;
      if (!newBrowser) {
        continue;
      }
      const initialTabs = [...newBrowser.tabs];
      const result = await restoreTabsIntoWindow(captured.tabs, {
        gBrowser: newBrowser,
        resolveWorkspaceId: () => options.assignWorkspaceId,
      });
      outcomes.push(...result);

      // Only discard the placeholder tabs once at least one restore succeeded,
      // and never remove the last remaining tab.
      if (!result.some((entry) => entry.ok)) {
        continue;
      }
      for (const tab of initialTabs) {
        if (!tab.isConnected) {
          continue;
        }
        if (newBrowser.tabs.length <= 1) {
          break;
        }
        try {
          newBrowser.removeTab?.(tab);
        } catch (error) {
          console.error(
            "WorkspacesBackupService: failed to close placeholder tab",
            error,
          );
        }
      }
    }

    return outcomes;
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

  /** Single-flight retention cleanup. */
  public cleanupRetention(): Promise<void> {
    if (this.cleanupInFlight) {
      return this.cleanupInFlight;
    }
    const run = this.runCleanupRetention().finally(() => {
      if (this.cleanupInFlight === run) {
        this.cleanupInFlight = null;
      }
    });
    this.cleanupInFlight = run;
    return run;
  }

  private async runCleanupRetention(): Promise<void> {
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
      const stat = await IOUtils.stat(path);
      if ((stat.size ?? 0) > WORKSPACE_BACKUP_LIMITS.maxFileBytes) {
        console.error(
          "WorkspacesBackupService: skipping oversized backup",
          { path, size: stat.size },
        );
        return null;
      }
      const file = await this.readBackupFile(path);
      if (!file) {
        return null;
      }
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
        "WorkspacesBackupService: invalid or unsupported backup file",
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

  /**
   * Enumerates normal browser windows. Windows whose privacy cannot be
   * determined are excluded, and the capture is aborted so a failed check can
   * never turn into a private-data leak.
   */
  private getBrowserWindows(): BrowserWindow[] {
    const windows: BrowserWindow[] = [];
    let unknownCount = 0;
    const enumerator = Services.wm.getEnumerator("navigator:browser");
    while (enumerator.hasMoreElements()) {
      const window = enumerator.getNext() as BrowserWindow;
      if (!window || window.closed) {
        continue;
      }
      const verdict = getPrivacyVerdict(window);
      if (verdict === "unknown") {
        unknownCount++;
        continue;
      }
      if (verdict === "private") {
        continue;
      }
      if (!window.gBrowser) {
        continue;
      }
      windows.push(window);
    }
    if (unknownCount > 0) {
      throw new Error("private-detection-failed");
    }
    return windows;
  }

  /** Non-private window used as the restore destination. */
  private getRestoreTargetWindow(): BrowserWindow | null {
    const recent = Services.wm.getMostRecentWindow(
      "navigator:browser",
    ) as BrowserWindow | null;
    if (recent && !recent.closed && recent.gBrowser) {
      const verdict = getPrivacyVerdict(recent);
      if (verdict === "normal") {
        return recent;
      }
      if (verdict === "unknown") {
        throw new Error("private-detection-failed");
      }
    }
    for (const window of this.getBrowserWindows()) {
      if (window.gBrowser) {
        return window;
      }
    }
    return null;
  }

  private resolveCurrentWorkspaceId(): TWorkspaceID | null {
    const selected = selectedWorkspaceID();
    if (selected && this.host.isWorkspaceID(selected)) {
      return selected;
    }
    const defaultCandidate = workspacesDataStore.defaultID as TWorkspaceID;
    if (defaultCandidate && this.host.isWorkspaceID(defaultCandidate)) {
      return defaultCandidate;
    }
    return null;
  }

  private captureWorkspaceState(): WorkspaceBackupState | null {
    const order = [...workspacesDataStore.order] as TWorkspaceID[];
    const workspaces: WorkspaceBackupWorkspace[] = [];
    for (const workspaceId of order) {
      const workspace = this.host.getRawWorkspace(workspaceId);
      if (!workspace) {
        continue;
      }
      workspaces.push({
        workspaceId,
        name: workspace.name,
        userContextId: workspace.userContextId ?? 0,
        icon: workspace.icon ?? null,
      });
    }
    if (workspaces.length === 0) {
      return null;
    }
    const defaultCandidate = workspacesDataStore.defaultID as TWorkspaceID;
    return {
      workspaces,
      order,
      defaultID: order.includes(defaultCandidate) ? defaultCandidate : null,
      currentWorkspaceId: this.resolveCurrentWorkspaceId(),
    };
  }

  private captureWindowTabs(
    window: BrowserWindow,
    sessionStore: Awaited<ReturnType<typeof ensureSessionStore>>,
    mode: WorkspaceBackupMode,
    isFocused: boolean,
    privateContainerUserContextId: number | null,
  ): WorkspaceBackupWindow | null {
    const gBrowser = window.gBrowser;
    if (!gBrowser) {
      return null;
    }
    const selectedTab = gBrowser.selectedTab;
    const tabs: WorkspaceBackupTab[] = [];

    for (const tab of gBrowser.tabs) {
      const userContextId = toNumber(tab.getAttribute("usercontextid"));
      // Private containers live in normal windows, so they must be excluded by
      // identity rather than by window privacy.
      if (
        privateContainerUserContextId !== null &&
        userContextId === privateContainerUserContextId
      ) {
        continue;
      }
      if (tab.hasAttribute("floorp-disablehistory")) {
        continue;
      }

      const state = parseTabState(sessionStore, tab);
      const snapshotTab = toWorkspaceSnapshotTab(
        tab,
        state,
        this.host,
        tab === selectedTab,
      );
      const backupTab: WorkspaceBackupTab = {
        state: sanitizeTabState(snapshotTab.state),
        title: snapshotTab.title,
        url: snapshotTab.url,
        pinned: snapshotTab.pinned,
        isSelected: tab === selectedTab,
        userContextId: snapshotTab.userContextId,
        lastShownWorkspaceId: snapshotTab.lastShownWorkspaceId,
      };
      if (mode === "workspaces") {
        const workspaceId = tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
        if (workspaceId && this.host.isWorkspaceID(workspaceId)) {
          backupTab.workspaceId = workspaceId;
        }
      }
      tabs.push(backupTab);
    }

    return { isFocused, tabs };
  }

  /**
   * Captures every normal browser window.
   *
   * In `workspaces` mode tabs carry their workspace attribution and the file
   * carries the workspace store. In `tabs` mode no workspace data is read at
   * all, so the backup stays correct even when the feature is disabled.
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
    const workspacesEnabled = isWorkspacesEnabled();
    const mode: WorkspaceBackupMode = workspacesEnabled ? "workspaces" : "tabs";
    const privateContainerUserContextId = getPrivateContainerUserContextId();

    const capturedWindows: WorkspaceBackupWindow[] = [];
    for (const window of windows) {
      const captured = this.captureWindowTabs(
        window,
        sessionStore,
        mode,
        window === focusedWindow,
        privateContainerUserContextId,
      );
      if (captured) {
        capturedWindows.push(captured);
      }
    }
    if (capturedWindows.length === 0) {
      throw new Error("no-browser-window");
    }

    const workspaceState = mode === "workspaces"
      ? this.captureWorkspaceState()
      : null;
    if (mode === "workspaces" && !workspaceState) {
      throw new Error("workspace-state-unavailable");
    }

    return buildBackupFile(
      kind,
      mode,
      Date.now(),
      capturedWindows,
      workspaceState ?? undefined,
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
    // A negative elapsed means the clock moved backwards; fall back to a full
    // interval instead of scheduling far into the future.
    const remaining = elapsed < 0 ? intervalMs : intervalMs - elapsed;
    const delay = delayMs ?? Math.max(1_000, remaining);
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
    // Workspaces may be disabled; unassigned tabs are still backed up.
    if (!this.config.enabled) {
      return;
    }
    const intervalMs = Math.max(1, this.config.intervalMinutes) * 60_000;
    const elapsed = Date.now() - readLastRunAt();
    if (elapsed >= 0 && elapsed < intervalMs) {
      return;
    }
    let windows: BrowserWindow[];
    try {
      windows = this.getBrowserWindows();
    } catch (error) {
      console.error(
        "WorkspacesBackupService: skipping scheduled backup",
        error,
      );
      return;
    }
    if (windows.length === 0) {
      return;
    }
    await this.backupNow("auto");
  }
}

/* -------------------------------------------------------------------------
 * Disk helpers
 * ---------------------------------------------------------------------- */

/**
 * Writes a backup and verifies it can be read back.
 *
 * `IOUtils.write` is the atomic variant in the pinned runtime: it accepts
 * `tmpPath` and `compress`; there is no `writeAtomic` method.
 */
const writeBackupFile = async (
  path: string,
  bytes: Uint8Array,
): Promise<void> => {
  const tmpPath = `${path}.tmp`;
  try {
    await IOUtils.write(path, bytes, {
      compress: true,
      tmpPath,
      flush: true,
    });
  } catch (error) {
    await IOUtils.remove(tmpPath, { ignoreAbsent: true }).catch(() => {});
    throw error;
  }

  try {
    const readBack = await IOUtils.read(path, { decompress: true });
    const parsed: unknown = JSON.parse(new TextDecoder().decode(readBack));
    if (!isWorkspaceBackupFile(parsed)) {
      throw new Error("backup-verify-failed");
    }
  } catch (error) {
    await IOUtils.remove(path, { ignoreAbsent: true }).catch(() => {});
    throw error;
  }
};
