// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// QA g6: real-timer scheduler checks across live windows (L-01, L-05..L-08)
// and future-dated backups after a clock rollback (T-03).
import { createRoot } from "solid-js";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import { withWindows } from "./workspace-last-tab-test-utils.ts";
import {
  type WorkspacesBackupHost,
  WorkspacesBackupService,
} from "../utils/workspaces-backup-service.ts";
import type { WorkspacesBackupFuncs } from "../utils/workspaces-backup-controller.ts";
import {
  formatBackupFileName,
  isWorkspaceBackupFile,
  parseBackupFileName,
} from "../utils/workspaces-backup-helpers.ts";
import {
  DEFAULT_WORKSPACE_BACKUP_CONFIG,
  type WorkspaceBackupConfig,
  type WorkspaceBackupFile,
  type WorkspaceBackupSummary,
} from "../utils/backup-types.ts";
import {
  WORKSPACE_BACKUP_CONFIG_PREF_NAME,
  WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
} from "../utils/workspaces-static-names.ts";
import type { TWorkspaceID } from "../utils/type.ts";
import type { TestWindow } from "./workspace-last-tab-test-types.ts";

const MINUTE = 60_000;
const DAY = 24 * 60 * MINUTE;
const NAME_PATTERN = /^backup-\d{8}-\d{6}-[0-9a-f]{8}\.jsonlz4$/;

type BackupWindow = TestWindow & {
  workspacesBackupFuncs?: WorkspacesBackupFuncs;
};

const detachedHost: WorkspacesBackupHost = {
  supportsWorkspaceRestore: false,
  isWorkspaceID: (_id: string): _id is TWorkspaceID => false,
  getRawWorkspace: () => undefined,
  restoreWorkspaceState: () => Promise.reject(new Error("workspaces-disabled")),
};

interface SavedPref {
  name: string;
  had: boolean;
  value: string;
}

const savePref = (name: string): SavedPref => ({
  name,
  had: Services.prefs.prefHasUserValue(name),
  value: Services.prefs.getStringPref(name, ""),
});

const restorePref = (saved: SavedPref): void => {
  if (saved.had) Services.prefs.setStringPref(saved.name, saved.value);
  else Services.prefs.clearUserPref(saved.name);
};

const setLastRun = (value: number): void =>
  Services.prefs.setStringPref(
    WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
    String(value),
  );

const setConfig = (config: Partial<WorkspaceBackupConfig>): void =>
  Services.prefs.setStringPref(
    WORKSPACE_BACKUP_CONFIG_PREF_NAME,
    JSON.stringify({ ...DEFAULT_WORKSPACE_BACKUP_CONFIG, ...config }),
  );

const backupDirectory = (): string =>
  PathUtils.join(PathUtils.profileDir, "workspaces", "backup");

async function listBackupNames(): Promise<Set<string>> {
  const directory = backupDirectory();
  if (!(await IOUtils.exists(directory))) return new Set();
  const children = await IOUtils.getChildren(directory);
  return new Set(
    children.map((path) => PathUtils.filename(path)).filter((name) =>
      NAME_PATTERN.test(name)
    ),
  );
}

async function newBackups(before: Set<string>): Promise<string[]> {
  return [...(await listBackupNames())].filter((name) => !before.has(name));
}

const delay = (ms: number): Promise<void> =>
  new Promise((resolve) => setTimeout(resolve, ms));

async function waitForAsync(
  predicate: () => Promise<boolean>,
  message: string,
  timeoutMs: number,
): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (await predicate()) return;
    await delay(200);
  }
  assert(await predicate(), message);
}

/** Restores the backup prefs and deletes every backup created meanwhile. */
async function withBackupState(
  check: (before: Set<string>) => Promise<void>,
): Promise<void> {
  const config = savePref(WORKSPACE_BACKUP_CONFIG_PREF_NAME);
  const lastRun = savePref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME);
  setLastRun(Date.now());
  const before = await listBackupNames();
  try {
    await check(before);
  } finally {
    // lastRun stays fresh while the schedulers react to the config restore,
    // so no backup is written after cleanup.
    setLastRun(Date.now());
    restorePref(config);
    await delay(1_500);
    for (const name of await newBackups(before)) {
      await IOUtils.remove(PathUtils.join(backupDirectory(), name));
    }
    restorePref(lastRun);
  }
}

const harnessFuncs = (): WorkspacesBackupFuncs => {
  const funcs = (window as Window & {
    workspacesBackupFuncs?: WorkspacesBackupFuncs;
  }).workspacesBackupFuncs;
  assert(funcs, "window backup API is available");
  return funcs;
};

async function summariesFor(
  names: string[],
): Promise<WorkspaceBackupSummary[]> {
  const all = await harnessFuncs().listBackups();
  return all.filter((summary) => names.includes(summary.fileName));
}

// L-01 with real timers: the run becomes due 3 s from now and fires once.
async function testDueRunFiresOnce(): Promise<void> {
  await withBackupState(async (before) => {
    setLastRun(Date.now() - 15 * MINUTE + 3_000);
    setConfig({ retentionDays: 8 });
    await waitForAsync(
      async () => (await newBackups(before)).length > 0,
      "scheduled backup appears once due",
      20_000,
    );
    await delay(4_000);
    const created = await newBackups(before);
    assertEquals(created.length, 1, "exactly one scheduled backup");
    const [summary] = await summariesFor(created);
    assert(summary, "backup is listed");
    assertEquals(summary.kind, "auto", "kind is auto");
  });
}

// L-05, L-06, L-08: three windows; disabled -> nothing; re-enabled -> exactly
// one backup in total (shared lastRun prevents duplicates).
async function testToggleAcrossWindows(): Promise<void> {
  await withBackupState(async (before) => {
    await withWindows(false, false, async (open) => {
      await open();
      await open();
      setLastRun(Date.now());
      setConfig({ enabled: false });
      setLastRun(Date.now() - 60 * MINUTE);
      await delay(4_000);
      assertEquals(
        (await newBackups(before)).length,
        0,
        "L-05 disabled: overdue schedule does not back up",
      );
      setConfig({ enabled: true });
      await waitForAsync(
        async () => (await newBackups(before)).length > 0,
        "L-06 re-enable backs up promptly",
        15_000,
      );
      await delay(5_000);
      const created = await newBackups(before);
      assertEquals(created.length, 1, "L-08 one backup across 3 windows");
      const [summary] = await summariesFor(created);
      assert(summary, "backup is listed");
      assertEquals(summary.kind, "auto", "kind is auto");
      assertEquals(summary.windowCount, 3, "capture spans all windows");
    });
  });
}

// L-07: with workspaces disabled the detached service still schedules auto
// backups in tabs mode.
async function testScheduleWithWorkspacesDisabled(): Promise<void> {
  await withBackupState(async (before) => {
    await withWindows(false, false, async (open) => {
      const win = await open() as BackupWindow;
      assert(!win.workspacesFuncs, "workspaces are disabled in the window");
      assert(win.workspacesBackupFuncs, "backup API exists without workspaces");
      setLastRun(Date.now() - 15 * MINUTE + 2_000);
      setConfig({ retentionDays: 8 });
      await waitForAsync(
        async () => (await newBackups(before)).length > 0,
        "auto backup appears with workspaces disabled",
        20_000,
      );
      await delay(3_000);
      const created = await newBackups(before);
      assertEquals(created.length, 1, "one auto backup");
      const [summary] = await summariesFor(created);
      assert(summary, "backup is listed");
      assertEquals(summary.kind, "auto", "kind is auto");
      assertEquals(summary.mode, "tabs", "tabs mode while disabled");
      assertEquals(summary.workspaceCount, 0, "no workspace data");
    }, false);
  });
}

function craftedFile(capturedAt: number): WorkspaceBackupFile {
  return {
    version: 2,
    kind: "auto",
    mode: "tabs",
    capturedAt,
    windows: [{
      isFocused: true,
      tabs: [{
        state: null,
        title: "QA g6",
        url: "https://example.com/qa-g6",
        pinned: false,
        isSelected: true,
        userContextId: 0,
        lastShownWorkspaceId: null,
      }],
    }],
  };
}

async function writeCrafted(capturedAt: number, shortId: string) {
  const file = craftedFile(capturedAt);
  assert(isWorkspaceBackupFile(file), "crafted file is valid");
  const name = formatBackupFileName(capturedAt, shortId);
  await IOUtils.write(
    PathUtils.join(backupDirectory(), name),
    new TextEncoder().encode(JSON.stringify(file)),
    { compress: true },
  );
  return name;
}

/** Moves existing backups aside so retention only sees crafted files. */
async function withIsolatedBackups(check: () => Promise<void>) {
  const directory = backupDirectory();
  const stash = PathUtils.join(
    PathUtils.profileDir,
    "workspaces",
    "qa-g6-stash",
  );
  await IOUtils.makeDirectory(directory, { ignoreExisting: true });
  await IOUtils.makeDirectory(stash, { ignoreExisting: true });
  const existing = [...(await listBackupNames())];
  for (const name of existing) {
    await IOUtils.move(
      PathUtils.join(directory, name),
      PathUtils.join(stash, name),
    );
  }
  const config = savePref(WORKSPACE_BACKUP_CONFIG_PREF_NAME);
  const lastRun = savePref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME);
  setLastRun(Date.now());
  try {
    await check();
  } finally {
    setLastRun(Date.now());
    restorePref(config);
    await delay(1_500);
    for (const name of await listBackupNames()) {
      await IOUtils.remove(PathUtils.join(directory, name));
    }
    for (const name of existing) {
      await IOUtils.move(
        PathUtils.join(stash, name),
        PathUtils.join(directory, name),
      );
    }
    await IOUtils.remove(stash, { recursive: true });
    restorePref(lastRun);
  }
}

// T-03: after the clock moved back, future-dated copies are listed first and
// survive until enough plausible copies exist; then they are pruned.
async function testFutureDatedBackupsAfterRollback(): Promise<void> {
  await withIsolatedBackups(async () => {
    const now = Date.now();
    const future = await writeCrafted(now + 30 * DAY, "f0000001");
    const recent = await writeCrafted(now - 1 * MINUTE, "a0000001");
    const listed = (await harnessFuncs().listBackups()).map((s) => s.fileName);
    assertEquals(
      listed.join(","),
      [future, recent].join(","),
      "future-dated copy is listed first (newest capturedAt)",
    );
    await createRoot(async (dispose) => {
      const service = new WorkspacesBackupService(detachedHost);
      try {
        await service.cleanupRetention();
        assert(
          (await listBackupNames()).has(future),
          "future copy kept while fewer than 3 plausible copies exist",
        );
        const fresh = await harnessFuncs().backupNow();
        const freshStamp = parseBackupFileName(fresh.fileName)?.timestamp;
        assert(
          freshStamp !== undefined && freshStamp <= Date.now() &&
            freshStamp >= now - MINUTE,
          "fresh copy uses the current clock",
        );
        const ordered = (await service.listBackups()).map((s) => s.fileName);
        assertEquals(ordered[0], future, "future copy still sorts first");
        assert(ordered.includes(fresh.fileName), "fresh copy listed");
        await service.cleanupRetention(fresh.fileName);
        assert((await listBackupNames()).has(future), "2 plausible: kept");
        await writeCrafted(now - 2 * MINUTE, "a0000002");
        await service.cleanupRetention(fresh.fileName);
        const remaining = await listBackupNames();
        assert(!remaining.has(future), "3 plausible copies: future pruned");
        assert(remaining.has(fresh.fileName), "fresh copy kept");
        assert(remaining.has(recent), "recent copy kept");
      } finally {
        dispose();
      }
    });
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG6Live.test.ts", [
    { name: "L-01 due run fires once (real timers)", fn: testDueRunFiresOnce },
    {
      name: "L-05/L-06/L-08 toggle across three windows",
      fn: testToggleAcrossWindows,
    },
    {
      name: "L-07 schedule with workspaces disabled",
      fn: testScheduleWithWorkspacesDisabled,
    },
    {
      name: "T-03 future-dated backups after rollback",
      fn: testFutureDatedBackupsAfterRollback,
    },
  ]);
}
