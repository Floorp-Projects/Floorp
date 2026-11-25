// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
//
// QA g2 (D-01..D-10): backup file writing and on-disk integrity.

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import { withWindows } from "./workspace-last-tab-test-utils.ts";
import {
  WORKSPACE_BACKUP_CONFIG_PREF_NAME,
  WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
} from "../utils/workspaces-static-names.ts";
import {
  formatBackupFileName,
  isBackupFileName,
  isWorkspaceBackupFile,
} from "../utils/workspaces-backup-helpers.ts";
import type { WorkspacesBackupFuncs } from "../utils/workspaces-backup-controller.ts";
import type { WorkspaceBackupSummary } from "../utils/backup-types.ts";
import type { TestWindow } from "./workspace-last-tab-test-types.ts";

type BackupWindow = TestWindow & {
  workspacesBackupFuncs?: WorkspacesBackupFuncs;
};

interface MutableIOUtils {
  read: typeof IOUtils.read;
  write: typeof IOUtils.write;
}

const BACKUP_DIR = PathUtils.join(
  PathUtils.profileDir,
  "workspaces",
  "backup",
);
const MOZLZ4_MAGIC = [0x6d, 0x6f, 0x7a, 0x4c, 0x7a, 0x34, 0x30, 0x00];
const MINUTE = 60 * 1000;

function hostFuncs(): WorkspacesBackupFuncs {
  const funcs = (globalThis as unknown as BackupWindow).workspacesBackupFuncs;
  assert(funcs, "harness window exposes the backup API");
  return funcs;
}

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function waitForAsync(
  predicate: () => Promise<boolean>,
  message: string,
  timeoutMs = 15_000,
): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (await predicate()) return;
    await sleep(100);
  }
  assert(await predicate(), message);
}

async function listDir(): Promise<string[]> {
  if (!(await IOUtils.exists(BACKUP_DIR))) return [];
  return (await IOUtils.getChildren(BACKUP_DIR)).map((path) =>
    PathUtils.filename(path)
  );
}

function dirPath(name: string): string {
  return PathUtils.join(BACKUP_DIR, name);
}

function sameBytes(a: Uint8Array, b: Uint8Array): boolean {
  if (a.length !== b.length) return false;
  for (let index = 0; index < a.length; index++) {
    if (a[index] !== b[index]) return false;
  }
  return true;
}

function savePref(name: string): () => void {
  const had = Services.prefs.prefHasUserValue(name);
  const value = had ? Services.prefs.getStringPref(name) : "";
  return () => {
    if (had) Services.prefs.setStringPref(name, value);
    else if (Services.prefs.prefHasUserValue(name)) {
      Services.prefs.clearUserPref(name);
    }
  };
}

/**
 * Snapshots the backup directory (regular files only) and the backup prefs,
 * and restores both afterwards: every file created by the test is removed and
 * every pre-existing file is written back byte for byte.
 */
async function withBackupDirSnapshot(
  check: () => Promise<void>,
): Promise<void> {
  const existed = await IOUtils.exists(BACKUP_DIR);
  const snapshot = new Map<string, Uint8Array>();
  for (const name of await listDir()) {
    const info = await IOUtils.stat(dirPath(name));
    if (info.type === "regular") {
      snapshot.set(name, await IOUtils.read(dirPath(name)));
    }
  }
  const restoreConfig = savePref(WORKSPACE_BACKUP_CONFIG_PREF_NAME);
  const restoreLastRun = savePref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME);
  try {
    await check();
  } finally {
    restoreConfig();
    restoreLastRun();
    // Let the config observer's retention pass finish before restoring files.
    await sleep(500);
    if (await IOUtils.exists(BACKUP_DIR)) {
      await IOUtils.setPermissions(BACKUP_DIR, 0o755).catch(() => {});
      for (const name of await listDir()) {
        if (!snapshot.has(name)) {
          await IOUtils.remove(dirPath(name), { recursive: true });
        }
      }
    }
    if (existed || snapshot.size > 0) {
      await IOUtils.makeDirectory(BACKUP_DIR, { createAncestors: true });
    } else if (await IOUtils.exists(BACKUP_DIR)) {
      await IOUtils.remove(BACKUP_DIR, { recursive: true });
    }
    for (const [name, bytes] of snapshot) {
      const path = dirPath(name);
      const current = (await IOUtils.exists(path))
        ? await IOUtils.read(path)
        : null;
      if (!current || !sameBytes(current, bytes)) {
        await IOUtils.write(path, bytes);
      }
    }
  }
}

async function backupNowSettled(): Promise<WorkspaceBackupSummary> {
  return await hostFuncs().backupNow();
}

function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

const STAMP_LENGTH = "backup-YYYYMMDD-HHMMSS".length;

// D-01
async function testLocationAndUtcFileName(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    const before = Date.now();
    const summary = await backupNowSettled();
    const after = Date.now();
    assert(
      /^backup-\d{8}-\d{6}-[0-9a-f]{8}\.jsonlz4$/.test(summary.fileName),
      `file name matches backup-YYYYMMDD-HHMMSS-xxxxxxxx.jsonlz4: ${summary.fileName}`,
    );
    assertEquals(summary.id, summary.fileName, "summary id is the file name");
    assert(
      await IOUtils.exists(dirPath(summary.fileName)),
      "file lives in <profile>/workspaces/backup",
    );
    assert(
      summary.capturedAt >= before && summary.capturedAt <= after,
      "capturedAt is the capture time",
    );
    const shortId = summary.fileName.slice(STAMP_LENGTH + 1, STAMP_LENGTH + 9);
    assertEquals(
      summary.fileName,
      formatBackupFileName(summary.capturedAt, shortId),
      "stamp in the name is the UTC rendering of capturedAt",
    );
    const date = new Date(summary.capturedAt);
    const utcDay = `${date.getUTCFullYear()}${
      String(date.getUTCMonth() + 1).padStart(2, "0")
    }${String(date.getUTCDate()).padStart(2, "0")}`;
    assertEquals(
      summary.fileName.slice("backup-".length, "backup-".length + 8),
      utcDay,
      "date part uses UTC",
    );
  });
}

// D-02
async function testMozLz4Format(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    const summary = await backupNowSettled();
    const raw = await IOUtils.read(dirPath(summary.fileName));
    assert(raw.length > 12, "file has a mozlz4 header and a body");
    for (let index = 0; index < MOZLZ4_MAGIC.length; index++) {
      assertEquals(raw[index], MOZLZ4_MAGIC[index], `magic byte ${index}`);
    }
    const declared = (raw[8] | (raw[9] << 8) | (raw[10] << 16) |
      (raw[11] << 24)) >>> 0;
    const decompressed = await IOUtils.read(dirPath(summary.fileName), {
      decompress: true,
    });
    assertEquals(
      declared,
      decompressed.length,
      "header declares the decompressed size",
    );
    const parsed: unknown = JSON.parse(new TextDecoder().decode(decompressed));
    assert(isWorkspaceBackupFile(parsed), "body is a valid v2 backup JSON");
    assertEquals(summary.fileSize, raw.length, "fileSize is the on-disk size");
  });
}

// D-03 (success path)
async function testReadBackMatchesSummary(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    const summary = await backupNowSettled();
    const parsed: unknown = JSON.parse(
      new TextDecoder().decode(
        await IOUtils.read(dirPath(summary.fileName), { decompress: true }),
      ),
    );
    assert(isWorkspaceBackupFile(parsed), "written file validates");
    assertEquals(parsed.capturedAt, summary.capturedAt, "capturedAt matches");
    assertEquals(parsed.kind, "manual", "kind is manual");
    assertEquals(parsed.mode, summary.mode, "mode matches");
    assertEquals(
      parsed.windows.length,
      summary.windowCount,
      "window count matches",
    );
    assertEquals(
      parsed.windows.reduce((sum, win) => sum + win.tabs.length, 0),
      summary.tabCount,
      "tab count matches",
    );
    const listed = (await hostFuncs().listBackups()).find((entry) =>
      entry.fileName === summary.fileName
    );
    assert(listed, "new backup is listed");
    assertEquals(
      JSON.stringify(listed),
      JSON.stringify(summary),
      "listed summary equals the summary returned by backupNow",
    );
  });
}

/** Replaces IOUtils.read/write in this window for the duration of `check`. */
async function withIOUtilsPatch(
  patch: Partial<MutableIOUtils>,
  check: () => Promise<void>,
): Promise<void> {
  const io = IOUtils as unknown as MutableIOUtils;
  const original: MutableIOUtils = { read: io.read, write: io.write };
  try {
    if (patch.read) io.read = patch.read;
    if (patch.write) io.write = patch.write;
    assert(
      (!patch.read || io.read === patch.read) &&
        (!patch.write || io.write === patch.write),
      "IOUtils methods can be patched in the chrome window",
    );
    await check();
  } finally {
    io.read = original.read;
    io.write = original.write;
  }
}

// D-03 (verification failure path)
async function testReadBackMismatchRemovesFile(): Promise<void> {
  for (const variant of ["content", "length"] as const) {
    await withBackupDirSnapshot(async () => {
      const before = new Set(await listDir());
      const io = IOUtils as unknown as MutableIOUtils;
      const realRead = io.read.bind(IOUtils);
      let corrupted = 0;
      await withIOUtilsPatch({
        read: async (path, opts) => {
          const bytes = await realRead(path, opts);
          const name = PathUtils.filename(path);
          if (
            opts?.decompress && path.startsWith(BACKUP_DIR) &&
            isBackupFileName(name) && !before.has(name) && corrupted === 0
          ) {
            corrupted++;
            if (variant === "length") return bytes.slice(0, -1);
            const copy = bytes.slice();
            copy[copy.length >> 1] ^= 0xff;
            return copy;
          }
          return bytes;
        },
      }, async () => {
        let message = "";
        try {
          await hostFuncs().backupNow();
        } catch (error) {
          message = errorMessage(error);
        }
        assertEquals(corrupted, 1, `${variant}: verification read happened`);
        assertEquals(
          message,
          `backup-verify-${variant}-mismatch`,
          `${variant}: backupNow rejects with the verify error`,
        );
      });
      const leftovers = (await listDir()).filter((name) => !before.has(name));
      assertEquals(
        leftovers.join(","),
        "",
        `${variant}: no backup or temp file is left behind`,
      );
    });
  }
}

async function writeTmp(name: string, ageMs: number): Promise<string> {
  const path = dirPath(name);
  await IOUtils.write(path, new TextEncoder().encode("partial"));
  await IOUtils.setModificationTime(path, Date.now() - ageMs);
  return path;
}

// D-04 / D-05
async function testStartupTmpCleanup(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    await IOUtils.makeDirectory(BACKUP_DIR, { createAncestors: true });
    const old = await writeTmp(
      "backup-20200101-000000-0a0b0c0d.jsonlz4.tmp",
      11 * MINUTE,
    );
    const veryOld = await writeTmp(
      "backup-20200101-000001-0a0b0c0e.jsonlz4.tmp",
      30 * 24 * 60 * MINUTE,
    );
    const fresh = await writeTmp(
      "backup-20200101-000002-0a0b0c0f.jsonlz4.tmp",
      0,
    );
    const almost = await writeTmp(
      "backup-20200101-000003-0a0b0c10.jsonlz4.tmp",
      9 * MINUTE,
    );
    const foreign = await writeTmp("notes.jsonlz4.tmp", 60 * MINUTE);
    // A new window constructs a new backup service, which runs the startup
    // orphan cleanup exactly like a browser restart does.
    await withWindows(false, false, async (open) => {
      await open();
      await waitForAsync(
        async () =>
          !(await IOUtils.exists(old)) && !(await IOUtils.exists(veryOld)),
        "D-04: .tmp files older than 10 minutes are removed at startup",
      );
    });
    assert(await IOUtils.exists(fresh), "D-05: a brand-new .tmp is kept");
    assert(await IOUtils.exists(almost), "D-05: a 9-minute-old .tmp is kept");
    assert(
      await IOUtils.exists(foreign),
      "a .tmp that does not match the strict backup pattern is kept",
    );
  });
}

// D-06
async function testUnrelatedFilesUntouched(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    await IOUtils.makeDirectory(BACKUP_DIR, { createAncestors: true });
    const keep = [
      "other.txt",
      "backup-notes.jsonlz4",
      "backup-20261124-120000-abcdef01.jsonlz4.bak",
      "backup-20261124-120000-abcdef0.jsonlz4",
      "xbackup-20261124-120000-abcdef01.jsonlz4",
    ];
    for (const name of keep) {
      await IOUtils.write(dirPath(name), new TextEncoder().encode(name));
    }
    await IOUtils.makeDirectory(dirPath("subdir"));
    await IOUtils.write(
      PathUtils.join(
        dirPath("subdir"),
        "backup-20261124-120000-abcdef02.jsonlz4",
      ),
      new TextEncoder().encode("nested"),
    );
    const own = await hostFuncs().backupNow();
    const oldTmp = await writeTmp(
      "backup-20261124-120000-abcdef03.jsonlz4.tmp",
      0,
    );

    // Retention cleanup: shortest retention, smallest pre-restore cap.
    Services.prefs.setStringPref(
      WORKSPACE_BACKUP_CONFIG_PREF_NAME,
      JSON.stringify({
        enabled: true,
        intervalMinutes: 1440,
        retentionDays: 1,
        maxPreRestoreBackups: 1,
      }),
    );
    await sleep(1500);
    for (const name of keep) {
      assert(await IOUtils.exists(dirPath(name)), `retention keeps ${name}`);
    }

    assert(await hostFuncs().deleteAllBackups(), "delete all succeeds");
    for (const name of keep) {
      assert(await IOUtils.exists(dirPath(name)), `delete all keeps ${name}`);
    }
    assert(
      await IOUtils.exists(
        PathUtils.join(
          dirPath("subdir"),
          "backup-20261124-120000-abcdef02.jsonlz4",
        ),
      ),
      "delete all does not recurse into sub directories",
    );
    assert(
      !(await IOUtils.exists(dirPath(own.fileName))),
      "delete all removes real backups",
    );
    assert(
      !(await IOUtils.exists(oldTmp)),
      "delete all removes backup temp files regardless of age",
    );
  });
}

// D-07
async function testMissingDirectoryIsCreated(): Promise<void> {
  const saved = `${BACKUP_DIR}.qa-g2-saved-${Date.now()}`;
  const existed = await IOUtils.exists(BACKUP_DIR);
  if (existed) await IOUtils.move(BACKUP_DIR, saved);
  try {
    assert(!(await IOUtils.exists(BACKUP_DIR)), "backup folder is absent");
    const restoreLastRun = savePref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME);
    try {
      const summary = await hostFuncs().backupNow();
      assert(await IOUtils.exists(BACKUP_DIR), "backup folder is created");
      assert(
        await IOUtils.exists(dirPath(summary.fileName)),
        "backup file is written into the new folder",
      );
    } finally {
      restoreLastRun();
    }
  } finally {
    if (await IOUtils.exists(BACKUP_DIR)) {
      await IOUtils.remove(BACKUP_DIR, { recursive: true });
    }
    if (existed) await IOUtils.move(saved, BACKUP_DIR);
  }
}

// D-08 (read-only folder)
async function testReadOnlyDirectory(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    await IOUtils.makeDirectory(BACKUP_DIR, { createAncestors: true });
    const before = new Set(await listDir());
    await IOUtils.setPermissions(BACKUP_DIR, 0o555, false);
    let message = "";
    try {
      await hostFuncs().backupNow();
    } catch (error) {
      message = errorMessage(error);
    } finally {
      await IOUtils.setPermissions(BACKUP_DIR, 0o755, false);
    }
    assert(message.length > 0, "backupNow rejects on a read-only folder");
    const leftovers = (await listDir()).filter((name) => !before.has(name));
    assertEquals(leftovers.join(","), "", "no partial file is left behind");
  });
}

// D-08 (disk full while writing the temp file)
async function testWriteFailureLeavesNothing(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    await IOUtils.makeDirectory(BACKUP_DIR, { createAncestors: true });
    const before = new Set(await listDir());
    const io = IOUtils as unknown as MutableIOUtils;
    const realWrite = io.write.bind(IOUtils);
    let injected = 0;
    await withIOUtilsPatch({
      write: async (path, data, options) => {
        if (options?.tmpPath && path.startsWith(BACKUP_DIR)) {
          injected++;
          // Simulate a write that got halfway before the disk filled up.
          await realWrite(options.tmpPath, data.slice(0, data.length >> 1));
          throw new DOMException(
            "Could not write: no space left on device",
            "NotReadableError",
          );
        }
        return await realWrite(path, data, options);
      },
    }, async () => {
      let message = "";
      try {
        await hostFuncs().backupNow();
      } catch (error) {
        message = errorMessage(error);
      }
      assertEquals(injected, 1, "the atomic write was attempted once");
      assert(/no space left/.test(message), `error surfaces: ${message}`);
    });
    const leftovers = (await listDir()).filter((name) => !before.has(name));
    assertEquals(
      leftovers.join(","),
      "",
      "neither the backup nor its .tmp is left behind",
    );
  });
}

// D-09
async function testTwoBackupsInOneSecond(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    let pair: WorkspaceBackupSummary[] = [];
    for (let attempt = 0; attempt < 10; attempt++) {
      const first = await hostFuncs().backupNow();
      const second = await hostFuncs().backupNow();
      pair = [first, second];
      if (
        first.fileName.slice(0, STAMP_LENGTH) ===
          second.fileName.slice(0, STAMP_LENGTH)
      ) break;
    }
    const [first, second] = pair;
    assertEquals(
      first.fileName.slice(0, STAMP_LENGTH),
      second.fileName.slice(0, STAMP_LENGTH),
      "two sequential backups landed in the same second",
    );
    assert(first.fileName !== second.fileName, "names differ by short id");
    assert(
      (await IOUtils.exists(dirPath(first.fileName))) &&
        (await IOUtils.exists(dirPath(second.fileName))),
      "both files exist",
    );
    const concurrent = await Promise.all([
      hostFuncs().backupNow(),
      hostFuncs().backupNow(),
    ]);
    assert(
      concurrent[0].fileName !== concurrent[1].fileName,
      "concurrent backups get distinct names",
    );
    const listed = new Set(
      (await hostFuncs().listBackups()).map((entry) => entry.fileName),
    );
    for (const summary of [first, second, ...concurrent]) {
      assert(listed.has(summary.fileName), `${summary.fileName} is listed`);
    }
  });
}

// D-10
async function testRetentionKeepsNewFile(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    Services.prefs.setStringPref(
      WORKSPACE_BACKUP_CONFIG_PREF_NAME,
      JSON.stringify({
        enabled: true,
        intervalMinutes: 1440,
        retentionDays: 1,
        maxPreRestoreBackups: 1,
      }),
    );
    await sleep(1000);
    // Crowd the retention window with future-dated copies (clock rollback),
    // which would otherwise outrank the fresh copy.
    await IOUtils.makeDirectory(BACKUP_DIR, { createAncestors: true });
    const valid = await hostFuncs().backupNow();
    const template: unknown = JSON.parse(
      new TextDecoder().decode(
        await IOUtils.read(dirPath(valid.fileName), { decompress: true }),
      ),
    );
    assert(isWorkspaceBackupFile(template), "template backup validates");
    for (let index = 0; index < 4; index++) {
      const capturedAt = Date.now() + (400 + index) * 24 * 60 * MINUTE;
      const name = formatBackupFileName(capturedAt, `f00d000${index}`);
      await IOUtils.write(
        dirPath(name),
        new TextEncoder().encode(
          JSON.stringify({ ...template, kind: "auto", capturedAt }),
        ),
        { compress: true },
      );
    }
    const summary = await hostFuncs().backupNow();
    assert(
      await IOUtils.exists(dirPath(summary.fileName)),
      "the backup just written survives the retention pass",
    );
    const listed = (await hostFuncs().listBackups()).map((entry) =>
      entry.fileName
    );
    assert(listed.includes(summary.fileName), "the new backup is listed");
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG2Files.test.ts", [
    { name: "D-01 location and UTC file name", fn: testLocationAndUtcFileName },
    { name: "D-02 mozlz4 format", fn: testMozLz4Format },
    {
      name: "D-03 read-back matches the summary",
      fn: testReadBackMatchesSummary,
    },
    {
      name: "D-03 read-back mismatch removes the file",
      fn: testReadBackMismatchRemovesFile,
    },
    { name: "D-04/D-05 startup .tmp cleanup", fn: testStartupTmpCleanup },
    {
      name: "D-06 unrelated files are untouched",
      fn: testUnrelatedFilesUntouched,
    },
    {
      name: "D-07 missing folder is created",
      fn: testMissingDirectoryIsCreated,
    },
    { name: "D-08 read-only folder", fn: testReadOnlyDirectory },
    {
      name: "D-08 write failure leaves nothing",
      fn: testWriteFailureLeavesNothing,
    },
    { name: "D-09 two backups in one second", fn: testTwoBackupsInOneSecond },
    {
      name: "D-10 retention keeps the new file",
      fn: testRetentionKeepsNewFile,
    },
  ]);
}
