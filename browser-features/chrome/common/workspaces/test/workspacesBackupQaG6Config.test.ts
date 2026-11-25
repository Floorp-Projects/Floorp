// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// QA g6: backup config pref (M-01..M-05) and time/zone handling (T-01, T-02).
import { createRoot } from "solid-js";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  formatBackupFileName,
  normalizeBackupConfig,
  parseBackupConfig,
  parseBackupFileName,
  sortBackupsNewestFirst,
} from "../utils/workspaces-backup-helpers.ts";
import {
  type WorkspacesBackupHost,
  WorkspacesBackupService,
} from "../utils/workspaces-backup-service.ts";
import {
  DEFAULT_WORKSPACE_BACKUP_CONFIG,
  type WorkspaceBackupConfig,
  type WorkspaceBackupSummary,
} from "../utils/backup-types.ts";
import {
  WORKSPACE_BACKUP_CONFIG_PREF_NAME,
  WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
} from "../utils/workspaces-static-names.ts";
import type { TWorkspaceID } from "../utils/type.ts";

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

function savePref(name: string): SavedPref {
  return {
    name,
    had: Services.prefs.prefHasUserValue(name),
    value: Services.prefs.getStringPref(name, ""),
  };
}

function restorePref(saved: SavedPref): void {
  if (saved.had) Services.prefs.setStringPref(saved.name, saved.value);
  else Services.prefs.clearUserPref(saved.name);
}

/**
 * Runs `check` with the config pref and lastRun restored afterwards. lastRun
 * is pinned to "now" so the windows' own schedulers, which react to every
 * config change, do not start a real backup while the pref is rewritten.
 */
async function withConfigPrefs(check: () => Promise<void> | void) {
  const config = savePref(WORKSPACE_BACKUP_CONFIG_PREF_NAME);
  const lastRun = savePref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME);
  Services.prefs.setStringPref(
    WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
    String(Date.now()),
  );
  try {
    await check();
  } finally {
    Services.prefs.setStringPref(
      WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
      String(Date.now()),
    );
    restorePref(config);
    // Let the schedulers' 1 s re-check pass while lastRun is still fresh.
    await new Promise((resolve) => setTimeout(resolve, 1_500));
    restorePref(lastRun);
  }
}

function withService<T>(fn: (service: WorkspacesBackupService) => T): T {
  return createRoot((dispose) => {
    const service = new WorkspacesBackupService(detachedHost);
    try {
      return fn(service);
    } finally {
      dispose();
    }
  });
}

function configFromPref(raw: string): WorkspaceBackupConfig {
  Services.prefs.setStringPref(WORKSPACE_BACKUP_CONFIG_PREF_NAME, raw);
  return withService((service) => service.getConfig());
}

function assertConfig(
  actual: WorkspaceBackupConfig,
  expected: WorkspaceBackupConfig,
  label: string,
): void {
  assertEquals(
    JSON.stringify(actual),
    JSON.stringify(expected),
    `${label}: config`,
  );
}

const defaults = (): WorkspaceBackupConfig => ({
  ...DEFAULT_WORKSPACE_BACKUP_CONFIG,
});

// M-01
async function testConfigStoredAsJsonPref(): Promise<void> {
  await withConfigPrefs(() => {
    const wanted: WorkspaceBackupConfig = {
      enabled: false,
      intervalMinutes: 30,
      retentionDays: 10,
      maxPreRestoreBackups: 12,
    };
    withService((service) => service.updateConfig(wanted));
    const raw = Services.prefs.getStringPref(
      WORKSPACE_BACKUP_CONFIG_PREF_NAME,
      "",
    );
    assertEquals(
      Services.prefs.getPrefType(WORKSPACE_BACKUP_CONFIG_PREF_NAME),
      Services.prefs.PREF_STRING,
      "floorp.workspaces.backup.config is a string pref",
    );
    assertConfig(JSON.parse(raw), wanted, "pref JSON");
    assertConfig(configFromPref(raw), wanted, "re-read config");
  });
}

// M-01: updateConfig normalizes before writing the pref.
async function testUpdateConfigWritesNormalizedJson(): Promise<void> {
  await withConfigPrefs(() => {
    withService((service) =>
      service.updateConfig({
        enabled: true,
        intervalMinutes: 99999,
        retentionDays: 0,
        maxPreRestoreBackups: 7.8,
      })
    );
    const stored = JSON.parse(
      Services.prefs.getStringPref(WORKSPACE_BACKUP_CONFIG_PREF_NAME, ""),
    );
    assertConfig(stored, {
      enabled: true,
      intervalMinutes: 1440,
      retentionDays: 1,
      maxPreRestoreBackups: 7,
    }, "normalized pref JSON");
  });
}

// M-02
async function testOutOfRangeValuesAreClamped(): Promise<void> {
  await withConfigPrefs(() => {
    assertConfig(
      configFromPref(JSON.stringify({
        enabled: true,
        intervalMinutes: 0,
        retentionDays: 0,
        maxPreRestoreBackups: 0,
      })),
      {
        enabled: true,
        intervalMinutes: 1,
        retentionDays: 1,
        maxPreRestoreBackups: 1,
      },
      "lower bounds",
    );
    assertConfig(
      configFromPref(JSON.stringify({
        enabled: true,
        intervalMinutes: 99999,
        retentionDays: 400,
        maxPreRestoreBackups: 99,
      })),
      {
        enabled: true,
        intervalMinutes: 1440,
        retentionDays: 365,
        maxPreRestoreBackups: 50,
      },
      "upper bounds",
    );
    assertConfig(
      configFromPref(JSON.stringify({
        enabled: true,
        intervalMinutes: 1440,
        retentionDays: 365,
        maxPreRestoreBackups: 50,
      })),
      {
        enabled: true,
        intervalMinutes: 1440,
        retentionDays: 365,
        maxPreRestoreBackups: 50,
      },
      "max boundary kept",
    );
  });
}

// M-02: a live service picks up a clamped value through its pref observer.
async function testLiveConfigObserverClamps(): Promise<void> {
  await withConfigPrefs(() => {
    createRoot((dispose) => {
      try {
        const service = new WorkspacesBackupService(detachedHost);
        Services.prefs.setStringPref(
          WORKSPACE_BACKUP_CONFIG_PREF_NAME,
          JSON.stringify({ ...defaults(), intervalMinutes: -10 }),
        );
        assertEquals(
          service.getConfig().intervalMinutes,
          1,
          "observer clamps negative interval to 1",
        );
        Services.prefs.setStringPref(
          WORKSPACE_BACKUP_CONFIG_PREF_NAME,
          JSON.stringify({ ...defaults(), intervalMinutes: 5000 }),
        );
        assertEquals(
          service.getConfig().intervalMinutes,
          1440,
          "observer clamps huge interval to 1440",
        );
      } finally {
        dispose();
      }
    });
  });
}

// M-03
async function testInvalidJsonFallsBackToDefaults(): Promise<void> {
  await withConfigPrefs(() => {
    for (const raw of ["{broken", "", "[]", "null", "42", '"text"', "{}"]) {
      assertConfig(configFromPref(raw), defaults(), `pref ${raw || "(empty)"}`);
    }
    Services.prefs.clearUserPref(WORKSPACE_BACKUP_CONFIG_PREF_NAME);
    assertConfig(
      withService((service) => service.getConfig()),
      defaults(),
      "missing pref",
    );
  });
}

// M-04
async function testNonIntegerValues(): Promise<void> {
  await withConfigPrefs(() => {
    assertConfig(
      configFromPref(
        '{"enabled":true,"intervalMinutes":2.9,"retentionDays":7.99,"maxPreRestoreBackups":1.5}',
      ),
      {
        enabled: true,
        intervalMinutes: 2,
        retentionDays: 7,
        maxPreRestoreBackups: 1,
      },
      "fractions are floored",
    );
    assertConfig(
      configFromPref(
        '{"enabled":"yes","intervalMinutes":"30","retentionDays":null,"maxPreRestoreBackups":true}',
      ),
      defaults(),
      "wrong types fall back per field",
    );
    assertConfig(
      configFromPref('{"intervalMinutes":1e400,"retentionDays":-1e400}'),
      defaults(),
      "Infinity falls back to default",
    );
    assertConfig(
      configFromPref('{"intervalMinutes":NaN}'),
      defaults(),
      "NaN literal is invalid JSON and falls back",
    );
    assertConfig(
      normalizeBackupConfig({
        enabled: false,
        intervalMinutes: Number.NaN,
        retentionDays: Number.POSITIVE_INFINITY,
        maxPreRestoreBackups: -0.5,
      }),
      {
        enabled: false,
        intervalMinutes: 15,
        retentionDays: 7,
        maxPreRestoreBackups: 1,
      },
      "NaN/Infinity default, negative fraction clamps",
    );
  });
}

// M-05 (chrome side): an out-of-preset maxPreRestoreBackups survives a
// read/update round trip that only changes other fields.
async function testMaxPreRestoreBackupsKept(): Promise<void> {
  await withConfigPrefs(() => {
    Services.prefs.setStringPref(
      WORKSPACE_BACKUP_CONFIG_PREF_NAME,
      JSON.stringify({ ...defaults(), maxPreRestoreBackups: 20 }),
    );
    withService((service) => {
      const config = service.getConfig();
      assertEquals(config.maxPreRestoreBackups, 20, "20 is read");
      service.updateConfig({ ...config, intervalMinutes: 30 });
    });
    const stored = JSON.parse(
      Services.prefs.getStringPref(WORKSPACE_BACKUP_CONFIG_PREF_NAME, ""),
    ) as WorkspaceBackupConfig;
    assertEquals(stored.maxPreRestoreBackups, 20, "20 is kept after save");
    assertEquals(stored.intervalMinutes, 30, "changed field is saved");
    assertEquals(
      parseBackupConfig(JSON.stringify(stored)).maxPreRestoreBackups,
      20,
      "parse keeps 20",
    );
  });
}

// T-01
function testFileNameUsesUtc(): void {
  const capturedAt = Date.UTC(2026, 10, 24, 23, 30, 5, 999);
  const fileName = formatBackupFileName(capturedAt, "0123abcd");
  assertEquals(
    fileName,
    "backup-20261124-233005-0123abcd.jsonlz4",
    "UTC date/time even when local (JST) date is already the 25th",
  );
  const parsed = parseBackupFileName(fileName);
  assert(parsed, "name parses");
  assertEquals(
    parsed.timestamp,
    Math.floor(capturedAt / 1000) * 1000,
    "parsed timestamp is the UTC instant (ms truncated)",
  );
  const offsetMinutes = new Date(capturedAt).getTimezoneOffset();
  const localHour = new Date(capturedAt).getHours();
  const localMinutes = (((23 * 60 + 30 - offsetMinutes) % 1440) + 1440) %
    1440;
  assertEquals(
    localHour,
    Math.floor(localMinutes / 60),
    "local display hour differs from file name by the zone offset",
  );
}

// T-01: a real backup's file name matches its capturedAt in UTC.
async function testRealBackupNameMatchesCapturedAt(): Promise<void> {
  const funcs = (window as Window & {
    workspacesBackupFuncs?: {
      backupNow(): Promise<WorkspaceBackupSummary>;
      deleteBackup(fileName: string): Promise<boolean>;
    };
  }).workspacesBackupFuncs;
  assert(funcs, "backup API available");
  const lastRun = savePref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME);
  let created: string | null = null;
  try {
    const summary = await funcs.backupNow();
    created = summary.fileName;
    const parsed = parseBackupFileName(summary.fileName);
    assert(parsed, "created name parses");
    assertEquals(
      parsed.timestamp,
      Math.floor(summary.capturedAt / 1000) * 1000,
      "file name timestamp is capturedAt in UTC",
    );
    assertEquals(
      summary.fileName,
      formatBackupFileName(summary.capturedAt, parsed.shortId),
      "name is formatted from capturedAt with UTC fields",
    );
  } finally {
    if (created) await funcs.deleteBackup(created);
    restorePref(lastRun);
  }
}

function summary(
  fileName: string,
  capturedAt: number,
): WorkspaceBackupSummary {
  return {
    id: fileName,
    kind: "auto",
    mode: "tabs",
    capturedAt,
    windowCount: 1,
    tabCount: 1,
    workspaceCount: 0,
    fileName,
    fileSize: 1,
  };
}

// T-02: ordering uses the epoch capturedAt, never the local clock fields, so
// it cannot change with the zone. Names are only a tie-breaker.
function testOrderingIsZoneIndependent(): void {
  const base = Date.UTC(2026, 10, 24, 15, 0, 0);
  const items = [
    summary("backup-20261124-150000-aaaaaaaa.jsonlz4", base),
    // Name says "earlier" but capturedAt (authoritative) is later.
    summary("backup-20261124-140000-bbbbbbbb.jsonlz4", base + 60_000),
    summary("backup-20261124-160000-cccccccc.jsonlz4", base - 60_000),
    summary("backup-20261124-150000-dddddddd.jsonlz4", base),
  ];
  const order = sortBackupsNewestFirst(items).map((item) =>
    item.fileName.slice(23, 31)
  );
  assertEquals(
    order.join(","),
    "bbbbbbbb,dddddddd,aaaaaaaa,cccccccc",
    "sorted by capturedAt desc, name desc on ties",
  );
  const shuffled = [items[2], items[0], items[3], items[1]];
  assertEquals(
    sortBackupsNewestFirst(shuffled).map((item) => item.fileName).join(),
    sortBackupsNewestFirst(items).map((item) => item.fileName).join(),
    "order is stable regardless of input order",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG6Config.test.ts", [
    {
      name: "M-01 config is saved as JSON pref",
      fn: testConfigStoredAsJsonPref,
    },
    {
      name: "M-01 updateConfig writes normalized JSON",
      fn: testUpdateConfigWritesNormalizedJson,
    },
    {
      name: "M-02 out-of-range values clamp",
      fn: testOutOfRangeValuesAreClamped,
    },
    {
      name: "M-02 live pref observer clamps",
      fn: testLiveConfigObserverClamps,
    },
    {
      name: "M-03 invalid JSON falls back to defaults",
      fn: testInvalidJsonFallsBackToDefaults,
    },
    { name: "M-04 fraction/NaN/string values", fn: testNonIntegerValues },
    {
      name: "M-05 maxPreRestoreBackups 20 is kept",
      fn: testMaxPreRestoreBackupsKept,
    },
    { name: "T-01 file name is UTC", fn: testFileNameUsesUtc },
    {
      name: "T-01 real backup name matches capturedAt",
      fn: testRealBackupNameMatchesCapturedAt,
    },
    {
      name: "T-02 ordering is zone independent",
      fn: testOrderingIsZoneIndependent,
    },
  ]);
}
