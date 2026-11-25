// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
//
// QA group g5 (K: retention / cleanup / delete) for workspace backup v2.
// Every case runs against a stashed, otherwise empty backup directory and puts
// the user's files, the backup config and lastRun prefs back afterwards.
import { createRoot } from "solid-js";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import { waitFor } from "./workspace-last-tab-test-utils.ts";
import {
  buildBackupFile,
  formatBackupFileName,
} from "../utils/workspaces-backup-helpers.ts";
import {
  type WorkspacesBackupHost,
  WorkspacesBackupService,
} from "../utils/workspaces-backup-service.ts";
import type {
  WorkspaceBackupConfig,
  WorkspaceBackupKind,
} from "../utils/backup-types.ts";
import type { TWorkspace, TWorkspaceID } from "../utils/type.ts";
import {
  WORKSPACE_BACKUP_CONFIG_PREF_NAME,
  WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
} from "../utils/workspaces-static-names.ts";
import type { WorkspacesBackupFuncs } from "../utils/workspaces-backup-controller.ts";

const DAY = 24 * 60 * 60 * 1000;
const HOUR = 60 * 60 * 1000;

const backupDir = (): string =>
  PathUtils.join(PathUtils.profileDir, "workspaces", "backup");
const stashDir = (): string =>
  PathUtils.join(PathUtils.profileDir, "workspaces", "backup-qa-g5-stash");

let idCounter = 0;
const nextShortId = (): string => {
  idCounter++;
  return (0xa0000000 + idCounter).toString(16).slice(-8);
};

class StubHost implements WorkspacesBackupHost {
  public readonly supportsWorkspaceRestore = false;
  isWorkspaceID(_id: string): _id is TWorkspaceID {
    return false;
  }
  getRawWorkspace(_id: TWorkspaceID): TWorkspace | undefined {
    return undefined;
  }
  restoreWorkspaceState(): Promise<void> {
    return Promise.reject(new Error("workspaces-disabled"));
  }
}

interface ServiceHandle {
  service: WorkspacesBackupService;
  dispose(): void;
}

function createService(): ServiceHandle {
  let service: WorkspacesBackupService | null = null;
  const dispose = createRoot((disposeRoot) => {
    service = new WorkspacesBackupService(new StubHost());
    return disposeRoot;
  });
  const created = service as WorkspacesBackupService | null;
  assert(created, "backup service is created");
  return {
    service: created,
    dispose: () => {
      created.dispose();
      dispose();
    },
  };
}

async function writeBackup(
  kind: WorkspaceBackupKind,
  capturedAt: number,
): Promise<string> {
  const file = buildBackupFile(kind, "tabs", capturedAt, [{
    isFocused: true,
    tabs: [{
      state: null,
      title: "qa-g5",
      url: `https://example.com/qa-g5/${capturedAt}`,
      pinned: false,
      isSelected: true,
      userContextId: 0,
      lastShownWorkspaceId: null,
    }],
  }]);
  // Wall-clock names collide for a fixed capturedAt only if the id repeats.
  const fileName = formatBackupFileName(
    Math.min(Math.max(capturedAt, 0), Date.UTC(9999, 11, 31)),
    nextShortId(),
  );
  await IOUtils.write(
    PathUtils.join(backupDir(), fileName),
    new TextEncoder().encode(JSON.stringify(file)),
    { compress: true },
  );
  return fileName;
}

async function listNames(): Promise<string[]> {
  if (!(await IOUtils.exists(backupDir()))) return [];
  return (await IOUtils.getChildren(backupDir()))
    .map((path) => PathUtils.filename(path))
    .sort();
}

/** Captures console.error calls made while `run` is pending. */
async function captureErrors(run: () => Promise<void>): Promise<string[]> {
  const original = console.error;
  const captured: string[] = [];
  console.error = (...args: unknown[]) => {
    captured.push(
      args.map((arg) => arg instanceof Error ? arg.message : String(arg))
        .join(" "),
    );
    original.apply(console, args);
  };
  try {
    await run();
  } finally {
    console.error = original;
  }
  return captured.filter((text) => text.includes("WorkspacesBackupService"));
}

function setConfig(config: Partial<WorkspaceBackupConfig>): void {
  Services.prefs.setStringPref(
    WORKSPACE_BACKUP_CONFIG_PREF_NAME,
    JSON.stringify({
      enabled: false,
      intervalMinutes: 15,
      retentionDays: 7,
      maxPreRestoreBackups: 10,
      ...config,
    }),
  );
}

function harnessBackupFuncs(): WorkspacesBackupFuncs {
  const win = Services.wm.getMostRecentWindow("navigator:browser") as
    | (Window & { workspacesBackupFuncs?: WorkspacesBackupFuncs })
    | null;
  const funcs = win?.workspacesBackupFuncs;
  assert(funcs, "a browser window exposes the backup API");
  return funcs;
}

/**
 * Moves the existing backup files away, runs `check` against an empty
 * directory with automatic backups disabled, then restores files and prefs.
 */
async function withIsolatedBackups(
  check: (service: WorkspacesBackupService) => Promise<void>,
): Promise<void> {
  const hadConfig = Services.prefs.prefHasUserValue(
    WORKSPACE_BACKUP_CONFIG_PREF_NAME,
  );
  const previousConfig = Services.prefs.getStringPref(
    WORKSPACE_BACKUP_CONFIG_PREF_NAME,
    "",
  );
  const hadLastRun = Services.prefs.prefHasUserValue(
    WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
  );
  const previousLastRun = Services.prefs.getStringPref(
    WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
    "0",
  );
  // Disable the schedulers first so no window writes a backup mid-test.
  setConfig({ enabled: false, retentionDays: 365, maxPreRestoreBackups: 50 });
  await IOUtils.makeDirectory(backupDir(), { createAncestors: true });
  await IOUtils.remove(stashDir(), { recursive: true, ignoreAbsent: true });
  await IOUtils.makeDirectory(stashDir(), { createAncestors: true });
  for (const child of await IOUtils.getChildren(backupDir())) {
    await IOUtils.move(
      child,
      PathUtils.join(stashDir(), PathUtils.filename(child)),
    );
  }
  const handle = createService();
  try {
    await handle.service.cleanupRetention();
    await check(handle.service);
  } finally {
    handle.dispose();
    if (hadConfig) {
      Services.prefs.setStringPref(
        WORKSPACE_BACKUP_CONFIG_PREF_NAME,
        previousConfig,
      );
    } else {
      Services.prefs.clearUserPref(WORKSPACE_BACKUP_CONFIG_PREF_NAME);
    }
    // Re-enabling resets lastRun to 0; put the real value back immediately so
    // the windows' schedulers keep their original cadence.
    if (hadLastRun) {
      Services.prefs.setStringPref(
        WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
        previousLastRun,
      );
    } else {
      Services.prefs.clearUserPref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME);
    }
    // Let the cleanups triggered by the config restore finish first.
    await new Promise((resolve) => setTimeout(resolve, 1500));
    for (const child of await IOUtils.getChildren(backupDir())) {
      await IOUtils.remove(child, { recursive: true, ignoreAbsent: true });
    }
    for (const child of await IOUtils.getChildren(stashDir())) {
      await IOUtils.move(
        child,
        PathUtils.join(backupDir(), PathUtils.filename(child)),
      );
    }
    await IOUtils.remove(stashDir(), { recursive: true, ignoreAbsent: true });
  }
}

function assertSameNames(actual: string[], expected: string[], label: string) {
  assertEquals(
    JSON.stringify([...actual].sort()),
    JSON.stringify([...expected].sort()),
    label,
  );
}

// K-01: an auto/manual backup older than retentionDays is deleted once at
// least MIN_KEEP_BACKUPS newer ones exist, both at service start-up and after
// a backup is saved (captureAndWrite runs the same cleanup).
async function testK01ExpiredBackupDeleted(): Promise<void> {
  await withIsolatedBackups(async () => {
    setConfig({ retentionDays: 7 });
    const now = Date.now();
    const recent = [
      await writeBackup("auto", now - 1 * HOUR),
      await writeBackup("manual", now - 2 * HOUR),
      await writeBackup("auto", now - 1 * DAY),
    ];
    const expired = await writeBackup("auto", now - 8 * DAY);
    const startup = createService();
    try {
      // The constructor starts a cleanup; single-flight returns that run.
      await startup.service.cleanupRetention();
    } finally {
      startup.dispose();
    }
    assertSameNames(await listNames(), recent, "8-day-old backup is deleted");

    const expiredManual = await writeBackup("manual", now - 30 * DAY);
    const kept = await writeBackup("manual", now - 6 * DAY);
    const handle = createService();
    try {
      await handle.service.cleanupRetention();
    } finally {
      handle.dispose();
    }
    const names = await listNames();
    assert(!names.includes(expiredManual), "30-day-old manual is deleted");
    assert(names.includes(kept), "6-day-old manual is inside retention");
    assert(!names.includes(expired), "expired file stays deleted");
  });
}

// K-02: when every backup is past retention the newest three survive.
async function testK02NewestThreeKept(): Promise<void> {
  await withIsolatedBackups(async (service) => {
    setConfig({ retentionDays: 7 });
    const now = Date.now();
    const names: string[] = [];
    for (let age = 10; age <= 15; age++) {
      names.push(
        await writeBackup(age % 2 ? "auto" : "manual", now - age * DAY),
      );
    }
    await service.cleanupRetention();
    assertSameNames(
      await listNames(),
      names.slice(0, 3),
      "only the three newest expired backups remain",
    );
  });
}

// K-03: pre-restore copies ignore retentionDays and are only capped by
// maxPreRestoreBackups (oldest first); fresh ones are inside the grace period.
async function testK03PreRestoreCountOnly(): Promise<void> {
  await withIsolatedBackups(async (service) => {
    setConfig({ retentionDays: 1, maxPreRestoreBackups: 10 });
    const now = Date.now();
    const autos = [
      await writeBackup("auto", now - 1 * HOUR),
      await writeBackup("auto", now - 2 * HOUR),
      await writeBackup("auto", now - 3 * HOUR),
    ];
    const pre = [
      await writeBackup("pre-restore", now - 30 * DAY),
      await writeBackup("pre-restore", now - 60 * DAY),
      await writeBackup("pre-restore", now - 90 * DAY),
    ];
    await service.cleanupRetention();
    assertSameNames(
      await listNames(),
      [...autos, ...pre],
      "old pre-restore copies survive age-based retention",
    );

    setConfig({ retentionDays: 1, maxPreRestoreBackups: 1 });
    const fresh = await writeBackup("pre-restore", now - 60 * 1000);
    const handle = createService();
    try {
      await handle.service.cleanupRetention();
    } finally {
      handle.dispose();
    }
    assertSameNames(
      await listNames(),
      [...autos, fresh],
      "count cap removes the oldest pre-restore copies and keeps the newest",
    );

    // A second fresh copy exceeds the cap of 1 but is inside the 10 min grace.
    const fresh2 = await writeBackup("pre-restore", now - 30 * 1000);
    const graced = createService();
    try {
      await graced.service.cleanupRetention();
    } finally {
      graced.dispose();
    }
    const names = await listNames();
    assert(
      names.includes(fresh) && names.includes(fresh2),
      "pre-restore copies younger than the grace period are never deleted",
    );
  });
}

// K-04: changing retentionDays through the pref makes the live window's
// service clean up immediately.
async function testK04RetentionChangeAppliesImmediately(): Promise<void> {
  await withIsolatedBackups(async () => {
    setConfig({ retentionDays: 7 });
    const now = Date.now();
    const recent = [
      await writeBackup("auto", now - 1 * HOUR),
      await writeBackup("auto", now - 2 * HOUR),
      await writeBackup("manual", now - 3 * HOUR),
    ];
    const twoDays = await writeBackup("auto", now - 2 * DAY);
    await new Promise((resolve) => setTimeout(resolve, 300));
    assert(
      (await listNames()).includes(twoDays),
      "2-day-old file is kept at 7",
    );
    setConfig({ retentionDays: 1 });
    let names: string[] = [];
    await waitFor(() => {
      listNames().then((value) => (names = value));
      return names.length > 0 && !names.includes(twoDays);
    }, "window services delete the 2-day-old file after retention=1");
    assertSameNames(names, recent, "only the retention-1 survivors remain");
  });
}

// K-05: a future-dated backup is deleted only while at least three plausibly
// dated backups exist, and never holds one of the three kept slots.
async function testK05FutureDated(): Promise<void> {
  await withIsolatedBackups(async (service) => {
    setConfig({ retentionDays: 7 });
    const now = Date.now();
    const future = await writeBackup("auto", now + 5 * DAY);
    const a = await writeBackup("auto", now - 1 * HOUR);
    const b = await writeBackup("auto", now - 2 * HOUR);
    await service.cleanupRetention();
    assertSameNames(
      await listNames(),
      [future, a, b],
      "future file survives with only two plausible backups",
    );
    const c = await writeBackup("auto", now - 20 * DAY);
    const handle = createService();
    try {
      await handle.service.cleanupRetention();
    } finally {
      handle.dispose();
    }
    assertSameNames(
      await listNames(),
      [a, b, c],
      "future file is deleted once three plausible backups exist; the old " +
        "third backup keeps its kept slot",
    );
  });
}

// K-06 / K-08 (service level): deleteBackup removes only the named file,
// deleteAllBackups removes every canonical name and all temp files.
async function testK06K08DeleteApis(): Promise<void> {
  await withIsolatedBackups(async () => {
    const funcs = harnessBackupFuncs();
    const now = Date.now();
    const a = await writeBackup("auto", now - 1 * HOUR);
    const b = await writeBackup("manual", now - 2 * HOUR);
    const c = await writeBackup("pre-restore", now - 3 * HOUR);
    assertEquals(await funcs.deleteBackup(b), true, "deleteBackup succeeds");
    assertSameNames(await listNames(), [a, c], "only the named file is gone");
    assertEquals(
      await funcs.deleteBackup("../prefs.js"),
      false,
      "invalid names are refused",
    );
    const freshTmp = `${formatBackupFileName(now, nextShortId())}.tmp`;
    await IOUtils.writeUTF8(PathUtils.join(backupDir(), freshTmp), "tmp");
    const unrelated = "notes.txt";
    await IOUtils.writeUTF8(PathUtils.join(backupDir(), unrelated), "keep");
    assertEquals(await funcs.deleteAllBackups(), true, "delete all succeeds");
    assertSameNames(
      await listNames(),
      [unrelated],
      "all backups and fresh .tmp files are removed; foreign files stay",
    );
    assertEquals((await funcs.listBackups()).length, 0, "list is empty");
  });
}

// K-09: an unreadable file with a canonical name is not listed but is still
// removed by deleteAllBackups.
async function testK09CorruptFileDeletedByDeleteAll(): Promise<void> {
  await withIsolatedBackups(async (service) => {
    const now = Date.now();
    const good = await writeBackup("auto", now - HOUR);
    const corrupt = formatBackupFileName(now - 2 * HOUR, nextShortId());
    await IOUtils.writeUTF8(PathUtils.join(backupDir(), corrupt), "garbage");
    const listed = (await service.listBackups()).map((s) => s.fileName);
    assertSameNames(listed, [good], "corrupt file is not listed");
    assertSameNames(await listNames(), [good, corrupt], "both exist on disk");
    assertEquals(await service.deleteAllBackups(), true, "delete all succeeds");
    assertSameNames(await listNames(), [], "corrupt file is deleted too");
  });
}

// K-10: concurrent cleanups from several service instances (one per window)
// must not reject and should not log deletion errors.
async function testK10ConcurrentCleanup(): Promise<void> {
  await withIsolatedBackups(async () => {
    setConfig({ retentionDays: 7 });
    const now = Date.now();
    const keep = [
      await writeBackup("auto", now - 1 * HOUR),
      await writeBackup("auto", now - 2 * HOUR),
      await writeBackup("auto", now - 3 * HOUR),
    ];
    for (let index = 0; index < 20; index++) {
      await writeBackup("auto", now - (10 + index) * DAY);
    }
    const handles = [createService(), createService(), createService()];
    let results: PromiseSettledResult<void>[] = [];
    const errors = await captureErrors(async () => {
      try {
        results = await Promise.allSettled(
          handles.map((handle) => handle.service.cleanupRetention()),
        );
      } finally {
        for (const handle of handles) handle.dispose();
      }
    });
    assert(
      results.every((result) => result.status === "fulfilled"),
      "every concurrent cleanup resolves",
    );
    assertSameNames(await listNames(), keep, "expired files are all deleted");
    assertEquals(
      errors.length,
      0,
      `concurrent cleanup logs no errors: ${errors.slice(0, 3).join(" | ")}`,
    );
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG5Retention.test.ts", [
    { name: "K-01 expired backup is deleted", fn: testK01ExpiredBackupDeleted },
    { name: "K-02 newest three are kept", fn: testK02NewestThreeKept },
    {
      name: "K-03 pre-restore is capped by count only",
      fn: testK03PreRestoreCountOnly,
    },
    {
      name: "K-04 retention change cleans up immediately",
      fn: testK04RetentionChangeAppliesImmediately,
    },
    { name: "K-05 future-dated backups", fn: testK05FutureDated },
    { name: "K-06/K-08 delete APIs", fn: testK06K08DeleteApis },
    {
      name: "K-09 corrupt file is removed by delete all",
      fn: testK09CorruptFileDeletedByDeleteAll,
    },
    { name: "K-10 concurrent cleanup", fn: testK10ConcurrentCleanup },
  ]);
}
