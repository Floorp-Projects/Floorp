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
  FUTURE_CAPTURE_TOLERANCE_MS,
  getBaseName,
  isBackupFileName,
  isRestorableTabUrl,
  isTabStateWithinLimit,
  isWorkspaceBackupFile,
  isWorkspaceIdValue,
  normalizeBackupConfig,
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

/**
 * Clamps a captured string to the validator's limit.
 *
 * Capture must never emit a value the validator rejects: a single over-long
 * title would otherwise make `writeBackupFile` discard the whole backup and
 * break every subsequent automatic run.
 */
const truncate = (value: string | null, max: number): string | null =>
  value !== null && value.length > max ? value.slice(0, max) : value;

/** Backoff used when a scheduled run is skipped without writing anything. */
const SCHEDULE_SKIP_BACKOFF_MS = 5 * 60 * 1000;

/** Matches the parent actor lock TTL; see isRestoreInFlight(). */
const RESTORE_IN_FLIGHT_TTL_MS = 11 * 60 * 1000;

/** Largest delay `setTimeout` accepts without overflowing to ~0. */
const MAX_TIMER_DELAY_MS = 2 ** 31 - 1;

/**
 * Orphaned atomic-write temp files left by a crash during `IOUtils.write`.
 * Matched strictly so no other file in the directory is ever touched.
 */
const BACKUP_TMP_FILE_NAME_PATTERN =
  /^backup-\d{8}-\d{6}-[0-9a-f]{8}\.jsonlz4\.tmp$/i;

/**
 * Temp files younger than this may belong to a write still running in another
 * window, so startup cleanup leaves them alone.
 */
const ORPHAN_TMP_MIN_AGE_MS = 10 * 60 * 1000;

/**
 * mozlz4 header: the magic ("mozLz40" + NUL) followed by the decompressed
 * size as a little-endian uint32.
 */
const MOZLZ4_MAGIC = [0x6d, 0x6f, 0x7a, 0x4c, 0x7a, 0x34, 0x30, 0x00];
const MOZLZ4_HEADER_BYTES = 12;

/**
 * Reads the decompressed size from a mozlz4 header without decompressing.
 * Throws `backup-invalid` for anything that is not a mozlz4 file.
 */
const readMozLz4DecompressedSize = async (path: string): Promise<number> => {
  const header = await IOUtils.read(path, { maxBytes: MOZLZ4_HEADER_BYTES });
  if (header.length < MOZLZ4_HEADER_BYTES) {
    throw new Error("backup-invalid");
  }
  for (let index = 0; index < MOZLZ4_MAGIC.length; index++) {
    if (header[index] !== MOZLZ4_MAGIC[index]) {
      throw new Error("backup-invalid");
    }
  }
  return (header[8] | (header[9] << 8) | (header[10] << 16) |
    (header[11] << 24)) >>> 0;
};

/** A captured tab together with the element it came from. */
interface CapturedTabEntry {
  tab: XULElement;
  backupTab: WorkspaceBackupTab;
}

/**
 * Picks at most `budget` tabs, preferring the selected tab, then pinned tabs,
 * then the rest in tab order. The original order is preserved in the result.
 */
const selectTabsWithinBudget = (
  entries: CapturedTabEntry[],
  budget: number,
): CapturedTabEntry[] => {
  if (entries.length <= budget) {
    return entries;
  }
  if (budget <= 0) {
    return [];
  }
  const keep = new Set<number>();
  const passes: ((entry: CapturedTabEntry) => boolean)[] = [
    (entry) => entry.backupTab.isSelected,
    (entry) => entry.backupTab.pinned,
    () => true,
  ];
  for (const accept of passes) {
    for (let index = 0; index < entries.length; index++) {
      if (keep.size >= budget) {
        break;
      }
      if (accept(entries[index])) {
        keep.add(index);
      }
    }
  }
  return entries.filter((_, index) => keep.has(index));
};

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
 * Result of resolving Floorp's private container.
 *
 * `none` (no private container configured) is a normal state; `unknown`
 * (lookup threw) must abort the capture, because treating it as "none" would
 * persist private-container tabs.
 */
export type PrivateContainerLookup =
  | { status: "none" }
  | { status: "configured"; userContextId: number }
  | { status: "unknown" };

const lookupPrivateContainer = (): PrivateContainerLookup => {
  try {
    const id = PrivateContainer.getPrivateContainerUserContextId();
    if (typeof id === "number" && Number.isSafeInteger(id) && id > 0) {
      return { status: "configured", userContextId: id };
    }
    return { status: "none" };
  } catch (error) {
    console.error(
      "WorkspacesBackupService: private container lookup failed",
      error,
    );
    return { status: "unknown" };
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
 * Fresh pre-restore backups are exempt from retention. A restore in another
 * window may still be running, and its safety copy is only protected inside
 * that service instance.
 */
const PRE_RESTORE_GRACE_MS = 10 * 60 * 1000;

/**
 * Waits until the target window finished its delayed startup. `readyState`
 * alone is not enough: gBrowser and SessionStore are wired during delayed
 * startup, and touching them earlier races the window's own restore.
 *
 * Every resolution path shares the same timeout and window-close guard so a promise
 * that never settles cannot hang the restore forever.
 */
const waitForDelayedStartup = (window: DelayedStartupWindow): Promise<void> => {
  return new Promise<void>((resolve, reject) => {
    let settled = false;
    let observer: ((subject: nsISupports | null) => void) | null = null;
    let timer: ReturnType<typeof globalThis.setTimeout> | null = null;

    // Not an "unload" listener: OpenBrowserWindow() returns the window while
    // it still shows its initial about:blank document, whose unload fires as
    // soon as browser.xhtml replaces it.
    const closedObserver = (subject: nsISupports | null): void => {
      if ((subject as unknown) === window) {
        settle(new Error("window-closed"));
      }
    };
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
      try {
        Services.obs.removeObserver(closedObserver, "domwindowclosed");
      } catch (error) {
        console.error(
          "WorkspacesBackupService: failed to remove close observer",
          error,
        );
      }
    };
    function settle(error?: unknown): void {
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
    }

    timer = globalThis.setTimeout(() => {
      settle(new Error("window-startup-timeout"));
    }, WINDOW_STARTUP_TIMEOUT_MS);
    Services.obs.addObserver(closedObserver, "domwindowclosed");

    const startup = window.delayedStartupPromise ??
      window.gBrowserInit?.delayedStartupPromise;
    if (startup) {
      startup.then(() => settle()).catch((error) => {
        settle(error ?? new Error("window-startup-failed"));
      });
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
export interface RestoreWorkspaceStateOptions {
  /**
   * Tabs that must survive the replacement because the safety copy does not
   * cover them. They are parked on a restored workspace instead of closed.
   */
  preserveTabs?: ReadonlySet<XULElement>;
}

export interface WorkspacesBackupHost {
  /**
   * Present on WorkspacesService. Used to re-apply workspace tab visibility
   * after an additive restore; the detached host has none.
   */
  readonly tabManagerCtx?: { updateTabsVisibility(): void };
  /**
   * False when restoreWorkspaceState() always rejects (the detached host).
   * Lets a destructive restore fail before it writes a safety copy.
   */
  readonly supportsWorkspaceRestore?: boolean;
  isWorkspaceID(id: string): id is TWorkspaceID;
  getRawWorkspace(id: TWorkspaceID): TWorkspace | undefined;
  /** Replaces the whole workspace store from a validated backup file. */
  restoreWorkspaceState(
    file: WorkspaceBackupFile,
    options?: RestoreWorkspaceStateOptions,
  ): Promise<void>;
}

/** Options for restoring a backup as plain tabs. */
export interface RestoreTabsOptions {
  /** Recreate the captured window layout instead of flattening into one window. */
  splitWindows: boolean;
  /** Workspace assigned to every restored tab, or null to leave them unassigned. */
  assignWorkspaceId: TWorkspaceID | null;
}

/** Result of a tabs-mode restore. */
export interface RestoreTabsResult {
  outcomes: RestoreTabOutcome[];
  /** Number of captured windows that could not be opened. */
  windowFailures: number;
  /** Tabs that failed to restore (skipped tabs are not failures). */
  failedCount: number;
  /** Tabs intentionally not opened, e.g. for unsupported URLs. */
  skippedCount: number;
}

/** Result of `restoreBackup`, sent across the actor bridge as-is. */
export interface RestoreBackupResult {
  success: boolean;
  error?: string;
  /** Tabs skipped for unsupported URLs, when an additive restore ran. */
  skippedCount?: number;
}

const countFailures = (outcomes: RestoreTabOutcome[]): number =>
  outcomes.filter((outcome) => !outcome.ok && !outcome.skipped).length;

const countSkipped = (outcomes: RestoreTabOutcome[]): number =>
  outcomes.filter((outcome) => outcome.skipped === true).length;

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
  private readonly wakeObserver: () => void;
  private disposed = false;
  private cleanupInFlight: Promise<void> | null = null;
  /**
   * Per-window restore guard. The process-wide lock lives in the parent actor;
   * this one also covers callers that bypass the actor and keeps the scheduler
   * from capturing a half-restored state.
   */
  private restoreInFlight: symbol | null = null;
  private restoreStartedAt = 0;

  /**
   * Whether a restore is running here. A restore whose promise never settles
   * lapses after RESTORE_IN_FLIGHT_TTL_MS (matching the actor lock), so it
   * cannot suppress scheduled backups in every window forever.
   */
  public isRestoreInFlight(): boolean {
    if (!this.restoreInFlight) {
      return false;
    }
    const age = Date.now() - this.restoreStartedAt;
    return age >= 0 && age < RESTORE_IN_FLIGHT_TTL_MS;
  }

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
    // Gecko timers run on a monotonic clock that pauses during system sleep,
    // so a pending timer fires late by the sleep duration. Re-derive the delay
    // from the wall-clock lastRun on wake.
    this.wakeObserver = () => this.scheduleNext();
    Services.prefs.addObserver(
      WORKSPACE_BACKUP_CONFIG_PREF_NAME,
      this.configObserver,
    );
    Services.prefs.addObserver(
      WORKSPACE_ENABLED_PREF_NAME,
      this.enabledObserver,
    );
    Services.obs.addObserver(this.wakeObserver, "wake_notification");
    onCleanup(() => this.dispose());

    this.cleanupRetention().catch((error) => {
      console.error(
        "WorkspacesBackupService: startup retention cleanup failed",
        error,
      );
    });
    removeOrphanTmpFiles(ORPHAN_TMP_MIN_AGE_MS).catch((error) => {
      console.error(
        "WorkspacesBackupService: startup temp file cleanup failed",
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
    this.config = normalizeBackupConfig(config);
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
    // A capture taken mid-restore (in any window: the capture spans all of
    // them) would record a half-replaced state.
    if (this.isRestoreInFlightInAnyWindow()) {
      throw new Error("restore-in-progress");
    }
    const { summary } = await this.captureAndWrite(kind);
    return summary;
  }

  /**
   * Captures, writes and verifies a backup, and also returns the tab elements
   * that were captured. The restore path uses them to tell which tabs the
   * safety copy actually covers.
   */
  private async captureAndWrite(
    kind: WorkspaceBackupKind,
  ): Promise<{
    summary: WorkspaceBackupSummary;
    capturedTabs: Set<XULElement>;
  }> {
    if (kind !== "pre-restore") {
      // Write the run timestamp before capturing to reduce duplicate runs
      // across windows. See the class comment for the trade-off.
      writeLastRunAt(Date.now());
    }

    const { file, capturedTabs } = await this.captureAllWorkspaces(kind);
    if (!isWorkspaceBackupFile(file)) {
      // Never persist a file the reader would reject. A pre-restore copy that
      // cannot be listed or restored is worse than no copy at all, because the
      // destructive restore would proceed without a usable safety net.
      throw new Error("backup-capture-invalid");
    }
    const directory = getBackupDirectory();
    await ensureDirectory(directory);

    const fileName = formatBackupFileName(file.capturedAt, createShortId());
    const path = PathUtils.join(directory, fileName);
    const bytes = new TextEncoder().encode(JSON.stringify(file));
    await writeBackupFile(path, bytes);

    const stat = await IOUtils.stat(path);
    const fileSize = stat.size ?? 0;
    if (fileSize > WORKSPACE_BACKUP_LIMITS.maxFileBytes) {
      // The reader rejects files above this limit, so reporting success would
      // hand back an unusable safety copy.
      await IOUtils.remove(path, { ignoreAbsent: true }).catch(() => {});
      throw new Error("backup-too-large");
    }
    // The file just written must survive retention: a pre-restore copy that is
    // deleted before the destructive restore runs is worse than no copy.
    await this.cleanupRetention(fileName);
    if (!(await IOUtils.exists(path))) {
      throw new Error("backup-retention-removed-new-file");
    }
    return {
      summary: buildBackupSummary(fileName, file, fileSize),
      capturedTabs,
    };
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
        // Unreadable or unknown-version files still carry a valid name and are
        // removed here too, even though listBackups() cannot show them.
        if (!isBackupFileName(getBaseName(child))) {
          continue;
        }
        await IOUtils.remove(child);
      }
      await removeOrphanTmpFiles(0);
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
  public async restoreBackup(fileName: string): Promise<RestoreBackupResult> {
    if (!isBackupFileName(fileName)) {
      return { success: false, error: "invalid-file-name" };
    }
    if (this.isRestoreInFlight()) {
      return { success: false, error: "restore-in-progress" };
    }
    // A lapsed restore that settles late must not clear a newer one's flag.
    const token = Symbol("restore");
    this.restoreInFlight = token;
    this.restoreStartedAt = Date.now();
    try {
      return await this.runRestore(fileName);
    } finally {
      if (this.restoreInFlight === token) {
        this.restoreInFlight = null;
      }
    }
  }

  private async runRestore(fileName: string): Promise<RestoreBackupResult> {
    const loaded = await this.loadBackup(fileName);
    if ("error" in loaded) {
      return { success: false, error: loaded.error };
    }
    const file = loaded.file;

    const workspacesEnabled = isWorkspacesEnabled();
    const destructive = workspacesEnabled && file.mode === "workspaces";
    let restoreOptions: RestoreWorkspaceStateOptions | undefined;

    // The detached host (workspaces off at startup, or still migrating)
    // cannot rebuild the store. Refuse before writing a safety copy, so
    // retries do not pile up pre-restore files that push out real backups.
    if (destructive && this.host.supportsWorkspaceRestore === false) {
      return { success: false, error: "workspaces-disabled" };
    }

    // A safety copy is only needed when the restore replaces existing state.
    if (destructive) {
      let capturedTabs: Set<XULElement>;
      try {
        ({ capturedTabs } = await this.captureAndWrite("pre-restore"));
      } catch (error) {
        if (error instanceof Error && error.message === "nothing-to-back-up") {
          // No capturable tab exists, so there is nothing a safety copy could
          // hold. Every current tab is preserved below instead.
          capturedTabs = new Set();
        } else {
          console.error(
            "WorkspacesBackupService: failed to capture pre-restore backup",
            error,
          );
          return { success: false, error: "pre-restore-failed" };
        }
      }
      // Only tabs the safety copy covers may be replaced. Anything opened since
      // the copy (including a same-count swap, or tabs dropped by capture
      // limits) is parked on a restored workspace instead of being closed.
      let preserveTabs: Set<XULElement>;
      try {
        preserveTabs = this.collectTabsNotIn(capturedTabs);
      } catch (error) {
        console.error(
          "WorkspacesBackupService: failed to verify restore state",
          error,
        );
        return { success: false, error: "private-detection-failed" };
      }
      restoreOptions = { preserveTabs };
    }

    try {
      if (destructive) {
        await this.host.restoreWorkspaceState(file, restoreOptions);
        // restoreTabsIntoWindow skips exactly the tabs the URL allowlist
        // rejects, so the count can be derived from the file.
        const skippedCount = file.windows.reduce(
          (count, window) =>
            count +
            window.tabs.filter((tab) =>
              !!tab.url && !isRestorableTabUrl(tab.url)
            ).length,
          0,
        );
        return { success: true, skippedCount };
      }
      // Workspaces enabled + tabs file: add them to the current workspace,
      // flattened into this window. Workspaces disabled: never touch the store
      // and always keep the captured window layout, whatever the file mode.
      const result = await this.restoreTabs(
        file,
        workspacesEnabled
          ? {
            splitWindows: false,
            assignWorkspaceId: this.resolveCurrentWorkspaceId(),
          }
          : { splitWindows: true, assignWorkspaceId: null },
      );
      if (workspacesEnabled) {
        this.refreshTabsVisibility();
      }
      if (result.windowFailures > 0 || result.failedCount > 0) {
        return {
          success: false,
          error: "restore-partial-failure",
          skippedCount: result.skippedCount,
        };
      }
      return { success: true, skippedCount: result.skippedCount };
    } catch (error) {
      console.error("WorkspacesBackupService: restore failed", error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /**
   * Re-applies workspace visibility after tabs were added, so restored tabs
   * assigned to a hidden workspace do not stay visible.
   */
  private refreshTabsVisibility(): void {
    try {
      this.host.tabManagerCtx?.updateTabsVisibility();
    } catch (error) {
      console.error(
        "WorkspacesBackupService: failed to update tab visibility",
        error,
      );
    }
  }

  /** Restores a backup as plain tabs (no workspace store changes). */
  public async restoreTabs(
    file: WorkspaceBackupFile,
    options: RestoreTabsOptions,
  ): Promise<RestoreTabsResult> {
    const currentWindow = this.getRestoreTargetWindow();
    const currentBrowser = currentWindow?.gBrowser;
    if (!currentWindow || !currentBrowser) {
      throw new Error("no-browser-window");
    }

    // An additive restore never maps workspace IDs, so a stored last-shown
    // marker would point at an unrelated live workspace (or none) and make
    // changeWorkspace select a tab from another workspace. Drop it.
    const windows = file.windows.map((window) => ({
      ...window,
      tabs: window.tabs.map((tab) => ({ ...tab, lastShownWorkspaceId: null })),
    }));
    const outcomes: RestoreTabOutcome[] = [];
    let windowFailures = 0;

    if (!options.splitWindows || windows.length <= 1) {
      const tabs = windows.flatMap((window) => window.tabs);
      const flatOutcomes = await restoreTabsIntoWindow(tabs, {
        gBrowser: currentBrowser,
        resolveWorkspaceId: () => options.assignWorkspaceId,
      });
      return {
        outcomes: flatOutcomes,
        windowFailures: 0,
        failedCount: countFailures(flatOutcomes),
        skippedCount: countSkipped(flatOutcomes),
      };
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

    let flatIndex = 0;
    for (let position = 0; position < order.length; position++) {
      const captured = windows[order[position]];
      const startIndex = flatIndex;
      flatIndex += captured.tabs.length;

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
        windowFailures++;
        outcomes.push({
          index: startIndex,
          ok: false,
          error: "window-open-failed",
        });
        continue;
      }
      const newBrowser = newWindow.gBrowser;
      if (!newBrowser) {
        windowFailures++;
        outcomes.push({
          index: startIndex,
          ok: false,
          error: "window-unavailable",
        });
        continue;
      }
      const initialTabs = [...newBrowser.tabs];
      const result = await restoreTabsIntoWindow(captured.tabs, {
        gBrowser: newBrowser,
        resolveWorkspaceId: () => options.assignWorkspaceId,
      });
      outcomes.push(...result);

      // Only discard the placeholder tabs once no tab in this window failed and
      // at least one was actually opened (skipped tabs are not failures), and
      // never remove the last remaining tab.
      if (
        countFailures(result) > 0 || !result.some((entry) => entry.ok)
      ) {
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

    return {
      outcomes,
      windowFailures,
      failedCount: countFailures(outcomes),
      skippedCount: countSkipped(outcomes),
    };
  }

  public dispose(): void {
    if (this.disposed) {
      return;
    }
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
    Services.obs.removeObserver(this.wakeObserver, "wake_notification");
  }

  /**
   * Single-flight retention cleanup.
   *
   * `protectedFileName` is never deleted, even when it falls outside the
   * retention window (for example when the clock moved backwards and existing
   * copies carry future timestamps).
   */
  public cleanupRetention(protectedFileName?: string): Promise<void> {
    if (this.cleanupInFlight) {
      return this.cleanupInFlight;
    }
    const run = this.runCleanupRetention(protectedFileName).finally(() => {
      if (this.cleanupInFlight === run) {
        this.cleanupInFlight = null;
      }
    });
    this.cleanupInFlight = run;
    return run;
  }

  private async runCleanupRetention(
    protectedFileName?: string,
  ): Promise<void> {
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
      now,
    );

    const toDelete = new Set<string>();
    for (const summary of [...expired, ...excessPreRestore]) {
      if (summary.fileName === protectedFileName) {
        continue;
      }
      // Another window's service instance may be running a restore right now.
      // Its safety copy is only protected inside that instance, so keep fresh
      // pre-restore files regardless of which window cleans up.
      // A future-dated copy (clock rollback / crafted file) is not "fresh".
      if (
        summary.kind === "pre-restore" &&
        summary.capturedAt <= now + FUTURE_CAPTURE_TOLERANCE_MS &&
        now - summary.capturedAt < PRE_RESTORE_GRACE_MS
      ) {
        continue;
      }
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
      const { file, size } = await this.readBackupFile(path);
      return buildBackupSummary(fileName, file, size);
    } catch (error) {
      console.error(
        "WorkspacesBackupService: skipping unreadable backup",
        { path, error },
      );
      return null;
    }
  }

  /**
   * Reads and validates a backup file. Both the compressed size and the
   * decompressed size declared in the mozlz4 header are bounded before any
   * decompression happens, so a hostile file cannot exhaust memory.
   *
   * Throws `backup-too-large`, `backup-invalid`, or an I/O error.
   */
  private async readBackupFile(
    path: string,
  ): Promise<{ file: WorkspaceBackupFile; size: number }> {
    const stat = await IOUtils.stat(path);
    const size = stat.size ?? 0;
    if (size > WORKSPACE_BACKUP_LIMITS.maxFileBytes) {
      console.error(
        "WorkspacesBackupService: refusing to read oversized backup",
        { path, size },
      );
      throw new Error("backup-too-large");
    }
    const decompressedSize = await readMozLz4DecompressedSize(path);
    if (decompressedSize > WORKSPACE_BACKUP_LIMITS.maxDecompressedBytes) {
      console.error(
        "WorkspacesBackupService: refusing to decompress oversized backup",
        { path, decompressedSize },
      );
      throw new Error("backup-too-large");
    }
    const bytes = await IOUtils.read(path, { decompress: true });
    if (bytes.length > WORKSPACE_BACKUP_LIMITS.maxDecompressedBytes) {
      throw new Error("backup-too-large");
    }
    const text = new TextDecoder().decode(bytes);
    const parsed: unknown = JSON.parse(text);
    if (!isWorkspaceBackupFile(parsed)) {
      console.error(
        "WorkspacesBackupService: invalid or unsupported backup file",
        path,
      );
      throw new Error("backup-invalid");
    }
    return { file: parsed, size };
  }

  private async loadBackup(
    fileName: string,
  ): Promise<{ file: WorkspaceBackupFile } | { error: string }> {
    const path = PathUtils.join(getBackupDirectory(), fileName);
    try {
      if (!(await IOUtils.exists(path))) {
        return { error: "backup-not-found" };
      }
      const { file } = await this.readBackupFile(path);
      return { file };
    } catch (error) {
      console.error("WorkspacesBackupService: failed to load backup", error);
      if (error instanceof Error && error.message === "backup-too-large") {
        return { error: "backup-too-large" };
      }
      // Unreadable and invalid files are not listed either, so the UI treats
      // them the same as a missing file.
      return { error: "backup-not-found" };
    }
  }

  /** Whether a restore is running in any browser window (including this one). */
  private isRestoreInFlightInAnyWindow(): boolean {
    if (this.isRestoreInFlight()) {
      return true;
    }
    let enumerator: ReturnType<typeof Services.wm.getEnumerator>;
    try {
      enumerator = Services.wm.getEnumerator("navigator:browser");
    } catch (error) {
      console.error(
        "WorkspacesBackupService: window enumeration failed",
        error,
      );
      // Unknown: prefer skipping one scheduled run over a torn capture.
      return true;
    }
    while (enumerator.hasMoreElements()) {
      // One misbehaving window must not block every scheduler, so a failing
      // lookup only skips that window.
      try {
        const window = enumerator.getNext() as Window & {
          workspacesBackupFuncs?: { isRestoreInFlight?: () => boolean };
        };
        if (window?.workspacesBackupFuncs?.isRestoreInFlight?.() === true) {
          return true;
        }
      } catch (error) {
        console.error(
          "WorkspacesBackupService: restore state lookup failed",
          error,
        );
      }
    }
    return false;
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
    // The service belongs to exactly one window. Prefer it over the most recent
    // window: the actor already pinned the requesting window, and a focus change
    // while the confirmation dialog is open must not move the restore to a
    // different window.
    const own = globalThis as unknown as BrowserWindow;
    if (own && !own.closed && own.gBrowser) {
      const ownVerdict = getPrivacyVerdict(own);
      if (ownVerdict === "normal") {
        return own;
      }
      if (ownVerdict === "unknown") {
        throw new Error("private-detection-failed");
      }
      throw new Error("private-window-unsupported");
    }

    // Detached contexts without their own browser window fall back to the most
    // recently used one.
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

  /**
   * Returns the tabs that are open now but were not part of the capture.
   *
   * Those tabs are not in the safety copy, so a destructive restore must not
   * close them. Comparing counts is not enough: closing one tab and opening
   * another keeps the count identical.
   */
  private collectTabsNotIn(capturedTabs: Set<XULElement>): Set<XULElement> {
    const preserve = new Set<XULElement>();
    for (const window of this.getBrowserWindows()) {
      for (const tab of window.gBrowser?.tabs ?? []) {
        if (!capturedTabs.has(tab)) {
          preserve.add(tab);
        }
      }
    }
    return preserve;
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

  /**
   * Captures the workspace store in a shape the validator accepts.
   *
   * The live store may carry duplicate or stale `order` entries, or workspaces
   * missing from `order`. Restore requires `order` to be an exact permutation of
   * the captured workspaces, so it is rebuilt here: live order first (filtered
   * and de-duplicated), then any remaining workspaces in data order. Counts and
   * string lengths are clamped to the validator's limits.
   */
  private captureWorkspaceState(): WorkspaceBackupState | null {
    const rawData: unknown = workspacesDataStore.data;
    const dataIds: TWorkspaceID[] = rawData instanceof Map
      ? [...(rawData as Map<TWorkspaceID, TWorkspace>).keys()]
      : [];
    const known = new Set<TWorkspaceID>(dataIds);
    const candidates: TWorkspaceID[] = [];
    const seen = new Set<TWorkspaceID>();
    const consider = (workspaceId: TWorkspaceID): void => {
      // The store's data keys are not validated on load; one malformed key
      // would otherwise make every backup fail validation.
      if (seen.has(workspaceId) || !isWorkspaceIdValue(workspaceId)) {
        return;
      }
      seen.add(workspaceId);
      candidates.push(workspaceId);
    };
    for (const workspaceId of workspacesDataStore.order as TWorkspaceID[]) {
      // Without a readable data map, fall back to the host lookup below.
      if (known.size === 0 || known.has(workspaceId)) {
        consider(workspaceId);
      }
    }
    for (const workspaceId of dataIds) {
      consider(workspaceId);
    }

    const workspaces: WorkspaceBackupWorkspace[] = [];
    for (const workspaceId of candidates) {
      if (workspaces.length >= WORKSPACE_BACKUP_LIMITS.maxWorkspaces) {
        console.warn(
          "WorkspacesBackupService: workspace count exceeds backup limit",
          {
            total: candidates.length,
            kept: WORKSPACE_BACKUP_LIMITS.maxWorkspaces,
          },
        );
        break;
      }
      const workspace = this.host.getRawWorkspace(workspaceId);
      if (!workspace) {
        continue;
      }
      const icon = workspace.icon ?? null;
      workspaces.push({
        workspaceId,
        name: truncate(
          workspace.name,
          WORKSPACE_BACKUP_LIMITS.maxNameLength,
        ) ?? "",
        userContextId: workspace.userContextId ?? 0,
        // A cut-off icon (often a data URL) is useless; drop it instead.
        icon: icon !== null &&
            icon.length > WORKSPACE_BACKUP_LIMITS.maxIconLength
          ? null
          : icon,
      });
    }
    if (workspaces.length === 0) {
      return null;
    }
    const order = workspaces.map((workspace) => workspace.workspaceId);
    const kept = new Set(order);
    const defaultCandidate = workspacesDataStore.defaultID as TWorkspaceID;
    const current = this.resolveCurrentWorkspaceId();
    return {
      workspaces,
      order,
      defaultID: kept.has(defaultCandidate) ? defaultCandidate : null,
      currentWorkspaceId: current !== null && kept.has(current)
        ? current
        : null,
    };
  }

  /**
   * Captures one window's tabs. Elements are returned alongside the data so the
   * caller can apply count limits before deciding which tabs are covered.
   */
  private captureWindowTabs(
    window: BrowserWindow,
    sessionStore: Awaited<ReturnType<typeof ensureSessionStore>>,
    mode: WorkspaceBackupMode,
    privateContainerUserContextId: number | null,
  ): CapturedTabEntry[] | null {
    const gBrowser = window.gBrowser;
    if (!gBrowser) {
      return null;
    }
    const selectedTab = gBrowser.selectedTab;
    const entries: CapturedTabEntry[] = [];

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
      // A tab the restore allowlist would skip (local files, internal pages
      // such as the settings page itself) cannot come back from this file.
      // Leave it out so it is not in `capturedTabs`: a destructive restore
      // then preserves it instead of closing it on the promise of a copy.
      if (snapshotTab.url && !isRestorableTabUrl(snapshotTab.url)) {
        continue;
      }
      // The validator rejects oversized states, and a rejected tab discards the
      // whole backup. Drop the state instead: the tab still restores from its
      // URL.
      const sanitizedState = sanitizeTabState(snapshotTab.state);
      // Every history entry was unrestorable: the copy would hold only a blank
      // tab, so treat the tab like an unrestorable URL and preserve it.
      const rawEntries = snapshotTab.state?.entries;
      if (
        sanitizedState === null && Array.isArray(rawEntries) &&
        rawEntries.length > 0
      ) {
        continue;
      }
      const boundedState = isTabStateWithinLimit(sanitizedState)
        ? sanitizedState
        : null;
      if (sanitizedState !== null && boundedState === null) {
        console.warn(
          "WorkspacesBackupService: dropping oversized tab state",
          { url: snapshotTab.url },
        );
      }
      // Without a state the URL is the only copy of the page; a truncated URL
      // would be restored as a different page while the original tab is
      // closed, so leave such a tab out of the backup (and open).
      if (
        boundedState === null && snapshotTab.url !== null &&
        snapshotTab.url.length > WORKSPACE_BACKUP_LIMITS.maxUrlLength
      ) {
        continue;
      }
      const backupTab: WorkspaceBackupTab = {
        state: boundedState,
        title: truncate(
          snapshotTab.title,
          WORKSPACE_BACKUP_LIMITS.maxTitleLength,
        ),
        url: truncate(snapshotTab.url, WORKSPACE_BACKUP_LIMITS.maxUrlLength),
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
      entries.push({ tab, backupTab });
    }

    return entries;
  }

  /**
   * Captures every normal browser window.
   *
   * In `workspaces` mode tabs carry their workspace attribution and the file
   * carries the workspace store. In `tabs` mode no workspace data is read at
   * all, so the backup stays correct even when the feature is disabled.
   *
   * Count limits are enforced here rather than left to the validator: a user
   * above a limit would otherwise get `backup-capture-invalid` on every run and
   * could never take a pre-restore copy. The focused window is kept first,
   * then windows in enumeration order; within a window the selected and pinned
   * tabs are kept preferentially. Tabs left out are not in `capturedTabs`, so a
   * destructive restore preserves them instead of closing them.
   *
   * Throws `nothing-to-back-up` when no capturable tab exists.
   */
  private async captureAllWorkspaces(
    kind: WorkspaceBackupKind,
  ): Promise<{ file: WorkspaceBackupFile; capturedTabs: Set<XULElement> }> {
    const windows = this.getBrowserWindows();
    if (windows.length === 0) {
      throw new Error("no-browser-window");
    }
    const privateContainer = lookupPrivateContainer();
    if (privateContainer.status === "unknown") {
      // Fail closed: without a reliable container identity we cannot tell
      // private-container tabs apart from normal ones.
      throw new Error("private-container-detection-failed");
    }
    const privateContainerUserContextId =
      privateContainer.status === "configured"
        ? privateContainer.userContextId
        : null;

    const sessionStore = await ensureSessionStore();
    const focusedWindow = Services.wm.getMostRecentWindow(
      "navigator:browser",
    ) as BrowserWindow | null;
    const workspacesEnabled = isWorkspacesEnabled();
    const mode: WorkspaceBackupMode = workspacesEnabled ? "workspaces" : "tabs";

    const focusedIndex = windows.findIndex((window) =>
      window === focusedWindow
    );
    const orderedWindows = focusedIndex > 0
      ? [
        windows[focusedIndex],
        ...windows.filter((_, index) => index !== focusedIndex),
      ]
      : windows;

    const capturedTabs = new Set<XULElement>();
    const capturedWindows: WorkspaceBackupWindow[] = [];
    let seenTabs = 0;
    let seenWindows = 0;
    let remainingBudget: number = WORKSPACE_BACKUP_LIMITS.maxTotalTabs;
    for (const window of orderedWindows) {
      const entries = this.captureWindowTabs(
        window,
        sessionStore,
        mode,
        privateContainerUserContextId,
      );
      if (!entries || entries.length === 0) {
        // Windows with nothing capturable would restore as empty windows.
        continue;
      }
      seenWindows++;
      seenTabs += entries.length;
      if (capturedWindows.length >= WORKSPACE_BACKUP_LIMITS.maxWindows) {
        continue;
      }
      const kept = selectTabsWithinBudget(
        entries,
        Math.min(WORKSPACE_BACKUP_LIMITS.maxTabsPerWindow, remainingBudget),
      );
      if (kept.length === 0) {
        continue;
      }
      remainingBudget -= kept.length;
      for (const entry of kept) {
        capturedTabs.add(entry.tab);
      }
      capturedWindows.push({
        isFocused: window === focusedWindow,
        tabs: kept.map((entry) => entry.backupTab),
      });
    }
    if (capturedWindows.length === 0) {
      throw new Error("nothing-to-back-up");
    }
    if (
      capturedTabs.size < seenTabs || capturedWindows.length < seenWindows
    ) {
      console.warn("WorkspacesBackupService: backup truncated to limits", {
        windows: { total: seenWindows, kept: capturedWindows.length },
        tabs: { total: seenTabs, kept: capturedTabs.size },
      });
    }

    const workspaceState = mode === "workspaces"
      ? this.captureWorkspaceState()
      : null;
    if (mode === "workspaces" && !workspaceState) {
      throw new Error("workspace-state-unavailable");
    }
    if (workspaceState) {
      // Attribution to a workspace that is not in the file (stale, or dropped
      // by the workspace limit) is removed; restore would ignore it anyway.
      const keptWorkspaceIds = new Set(workspaceState.order);
      for (const capturedWindow of capturedWindows) {
        for (const tab of capturedWindow.tabs) {
          if (tab.workspaceId && !keptWorkspaceIds.has(tab.workspaceId)) {
            delete tab.workspaceId;
          }
        }
      }
    }

    return {
      file: buildBackupFile(
        kind,
        mode,
        Date.now(),
        capturedWindows,
        workspaceState ?? undefined,
      ),
      capturedTabs,
    };
  }

  private scheduleNext(delayMs?: number): void {
    if (this.timer !== null) {
      globalThis.clearTimeout(this.timer);
      this.timer = null;
    }
    if (this.disposed || !this.config.enabled) {
      return;
    }

    const intervalMs = this.getIntervalMs();
    const elapsed = Date.now() - readLastRunAt();
    // A negative elapsed means the clock moved backwards; fall back to a full
    // interval instead of scheduling far into the future.
    const remaining = elapsed < 0 ? intervalMs : intervalMs - elapsed;
    // setTimeout overflows above 2^31-1 ms and would fire immediately.
    const delay = Math.min(
      MAX_TIMER_DELAY_MS,
      delayMs ?? Math.max(1_000, remaining),
    );
    this.timer = globalThis.setTimeout(() => {
      this.timer = null;
      this.runScheduledBackup()
        .catch((error): undefined => {
          console.error(
            "WorkspacesBackupService: scheduled backup failed",
            error,
          );
          return undefined;
        })
        .then((nextDelay) => this.scheduleNext(nextDelay));
    }, delay);
  }

  private getIntervalMs(): number {
    // normalizeBackupConfig already clamps the interval; this guards against a
    // non-normalized value reaching the scheduler.
    return Math.max(1, this.config.intervalMinutes) * 60_000;
  }

  /**
   * Runs one scheduled backup if it is due.
   *
   * Returns a delay override when the run was skipped without updating
   * `lastRun`; without it the scheduler would retry every second.
   */
  private async runScheduledBackup(): Promise<number | undefined> {
    // Workspaces may be disabled; unassigned tabs are still backed up.
    if (!this.config.enabled) {
      return undefined;
    }
    const intervalMs = this.getIntervalMs();
    const elapsed = Date.now() - readLastRunAt();
    if (elapsed >= 0 && elapsed < intervalMs) {
      return undefined;
    }
    const backoffMs = Math.min(intervalMs, SCHEDULE_SKIP_BACKOFF_MS);
    // The capture spans every window, so a restore running in any of them
    // would be captured half-done.
    if (this.isRestoreInFlightInAnyWindow()) {
      return backoffMs;
    }
    let windows: BrowserWindow[];
    try {
      windows = this.getBrowserWindows();
    } catch (error) {
      console.error(
        "WorkspacesBackupService: skipping scheduled backup",
        error,
      );
      return backoffMs;
    }
    if (windows.length === 0) {
      // Only private windows are open.
      return backoffMs;
    }
    try {
      // lastRun is written before capture, so a failed or empty run waits a
      // full interval through the normal schedule.
      await this.backupNow("auto");
    } catch (error) {
      if (error instanceof Error && error.message === "nothing-to-back-up") {
        return undefined;
      }
      throw error;
    }
    return undefined;
  }
}

/* -------------------------------------------------------------------------
 * Disk helpers
 * ---------------------------------------------------------------------- */

/**
 * Writes a backup and verifies the bytes read back match what was written.
 *
 * `IOUtils.write` is the atomic variant in the pinned runtime: it accepts
 * `tmpPath` and `compress`; there is no `writeAtomic` method.
 */
const writeBackupFile = async (
  path: string,
  bytes: Uint8Array,
): Promise<void> => {
  // The reader rejects anything above this bound, so writing it would report
  // success for a safety copy that can never be restored.
  if (bytes.length > WORKSPACE_BACKUP_LIMITS.maxDecompressedBytes) {
    throw new Error("backup-too-large");
  }
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
    if (readBack.length !== bytes.length) {
      throw new Error("backup-verify-length-mismatch");
    }
    for (let index = 0; index < bytes.length; index++) {
      if (readBack[index] !== bytes[index]) {
        throw new Error("backup-verify-content-mismatch");
      }
    }
  } catch (error) {
    await IOUtils.remove(path, { ignoreAbsent: true }).catch(() => {});
    throw error;
  }
};

/**
 * Removes orphaned `backup-*.jsonlz4.tmp` files left by an interrupted atomic
 * write. Only names matching the strict temp pattern are touched. Files newer
 * than `minAgeMs` are kept because another window may still be writing them.
 */
const removeOrphanTmpFiles = async (minAgeMs: number): Promise<void> => {
  const directory = getBackupDirectory();
  if (!(await IOUtils.exists(directory))) {
    return;
  }
  const now = Date.now();
  for (const child of await IOUtils.getChildren(directory)) {
    if (!BACKUP_TMP_FILE_NAME_PATTERN.test(getBaseName(child))) {
      continue;
    }
    try {
      if (minAgeMs > 0) {
        const stat = await IOUtils.stat(child);
        const modified = stat.lastModified ?? now;
        if (now - modified < minAgeMs) {
          continue;
        }
      }
      await IOUtils.remove(child, { ignoreAbsent: true });
    } catch (error) {
      console.error(
        "WorkspacesBackupService: failed to remove orphan temp file",
        { path: child, error },
      );
    }
  }
};
