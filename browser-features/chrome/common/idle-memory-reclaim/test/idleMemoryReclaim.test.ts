// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  isIdleEnough,
  type MemorySnapshot,
  sanitizeSettings,
  sanitizeStats,
  shouldReclaim,
} from "../index.ts";
import {
  BYTES_PER_MB,
  DEFAULT_SETTINGS,
  DEFAULT_STATS,
  type IdleMemoryReclaimSettings,
  type IdleMemoryReclaimStats,
  MIN_IDLE_THRESHOLD_SEC,
  MIN_POLL_INTERVAL_SEC,
  MIN_RECLAIM_INTERVAL_SEC,
} from "../types.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function makeSettings(
  overrides: Partial<IdleMemoryReclaimSettings> = {},
): IdleMemoryReclaimSettings {
  return { ...DEFAULT_SETTINGS, ...overrides };
}

function makeStats(
  overrides: Partial<IdleMemoryReclaimStats> = {},
): IdleMemoryReclaimStats {
  return { ...DEFAULT_STATS, ...overrides };
}

function makeSnapshot(
  overrides: Partial<MemorySnapshot> = {},
): MemorySnapshot {
  return { residentBytes: 0, ghostWindows: 0, ...overrides };
}

// ---------------------------------------------------------------------------
// sanitizeSettings
// ---------------------------------------------------------------------------

function testSanitizeSettingsWithNull(): void {
  assertEquals(
    sanitizeSettings(null).idleThresholdSec,
    DEFAULT_SETTINGS.idleThresholdSec,
    "null should fall back to defaults",
  );
}

function testSanitizeSettingsWithNonObject(): void {
  assertEquals(
    sanitizeSettings("not an object").minIntervalSec,
    DEFAULT_SETTINGS.minIntervalSec,
    "non-object should fall back to defaults",
  );
}

function testSanitizeSettingsWithPartialObject(): void {
  const result = sanitizeSettings({ enabled: false });
  assertEquals(result.enabled, false, "explicit field should be honoured");
  assertEquals(
    result.minResidentMB,
    DEFAULT_SETTINGS.minResidentMB,
    "missing fields should fall back to defaults",
  );
}

function testSanitizeSettingsRejectsWrongTypes(): void {
  const result = sanitizeSettings({
    enabled: "yes",
    idleThresholdSec: "600",
    minResidentMB: null,
  });
  assertEquals(
    result.enabled,
    DEFAULT_SETTINGS.enabled,
    "string should not be accepted as boolean",
  );
  assertEquals(
    result.idleThresholdSec,
    DEFAULT_SETTINGS.idleThresholdSec,
    "string should not be accepted as number",
  );
  assertEquals(
    result.minResidentMB,
    DEFAULT_SETTINGS.minResidentMB,
    "null should not be accepted as number",
  );
}

function testSanitizeSettingsClampsIdleThreshold(): void {
  const result = sanitizeSettings({ idleThresholdSec: 1 });
  assertEquals(
    result.idleThresholdSec,
    MIN_IDLE_THRESHOLD_SEC,
    "idle threshold below the floor should be clamped",
  );
}

function testSanitizeSettingsClampsInterval(): void {
  const result = sanitizeSettings({ minIntervalSec: 0 });
  assertEquals(
    result.minIntervalSec,
    MIN_RECLAIM_INTERVAL_SEC,
    "interval below the floor should be clamped",
  );
}

function testSanitizeSettingsRejectsNonFinite(): void {
  const result = sanitizeSettings({
    idleThresholdSec: Number.NaN,
    minIntervalSec: Number.POSITIVE_INFINITY,
  });
  assertEquals(
    result.idleThresholdSec,
    DEFAULT_SETTINGS.idleThresholdSec,
    "NaN should fall back to the default",
  );
  assertEquals(
    result.minIntervalSec,
    DEFAULT_SETTINGS.minIntervalSec,
    "Infinity should fall back to the default",
  );
}

function testSanitizeSettingsClampsPollInterval(): void {
  const result = sanitizeSettings({ pollIntervalSec: 1 });
  assertEquals(
    result.pollIntervalSec,
    MIN_POLL_INTERVAL_SEC,
    "poll interval below the floor should be clamped",
  );
}

function testSanitizeSettingsAcceptsValidValues(): void {
  const result = sanitizeSettings({
    enabled: false,
    idleThresholdSec: 120,
    pollIntervalSec: 90,
    minIntervalSec: 600,
    minResidentMB: 900,
    reclaimOnGhostWindows: false,
  });
  assertEquals(result.enabled, false, "enabled should round-trip");
  assertEquals(result.idleThresholdSec, 120, "idleThresholdSec should pass");
  assertEquals(result.pollIntervalSec, 90, "pollIntervalSec should pass");
  assertEquals(result.minIntervalSec, 600, "minIntervalSec should pass");
  assertEquals(result.minResidentMB, 900, "minResidentMB should pass");
  assertEquals(
    result.reclaimOnGhostWindows,
    false,
    "reclaimOnGhostWindows should round-trip",
  );
}

// ---------------------------------------------------------------------------
// sanitizeStats
// ---------------------------------------------------------------------------

function testSanitizeStatsWithNull(): void {
  assertEquals(
    sanitizeStats(null).runCount,
    DEFAULT_STATS.runCount,
    "null should fall back to zeroed stats",
  );
}

function testSanitizeStatsPreservesNegativeFreed(): void {
  // Memory can grow across a reclaim, so a negative value must survive.
  const result = sanitizeStats({ lastFreedBytes: -1024 });
  assertEquals(
    result.lastFreedBytes,
    -1024,
    "negative lastFreedBytes should be preserved",
  );
}

function testSanitizeStatsClampsNegativeCounters(): void {
  const result = sanitizeStats({ runCount: -5, totalFreedBytes: -100 });
  assertEquals(result.runCount, 0, "runCount should never be negative");
  assertEquals(
    result.totalFreedBytes,
    0,
    "totalFreedBytes should never be negative",
  );
}

// ---------------------------------------------------------------------------
// shouldReclaim
// ---------------------------------------------------------------------------

function testShouldReclaimSkipsWhenDisabled(): void {
  const decision = shouldReclaim(
    makeSettings({ enabled: false }),
    makeSnapshot({ residentBytes: 8_000 * BYTES_PER_MB }),
    makeStats(),
    1_000_000,
  );
  assert(!decision, "disabled feature must never reclaim");
}

function testShouldReclaimSkipsWhenThrottled(): void {
  const now = 1_000_000;
  const decision = shouldReclaim(
    makeSettings({ minIntervalSec: 300, minResidentMB: 0 }),
    makeSnapshot({ residentBytes: 8_000 * BYTES_PER_MB }),
    // Ran 10 seconds ago, well inside the throttle window.
    makeStats({ lastRunAt: now - 10_000 }),
    now,
  );
  assert(!decision, "reclaim within the throttle window must be skipped");
}

function testShouldReclaimRunsAfterThrottleExpires(): void {
  const now = 10_000_000;
  const decision = shouldReclaim(
    makeSettings({ minIntervalSec: 300, minResidentMB: 100 }),
    makeSnapshot({ residentBytes: 500 * BYTES_PER_MB }),
    makeStats({ lastRunAt: now - 301_000 }),
    now,
  );
  assert(decision, "reclaim should run once the throttle window has passed");
}

function testShouldReclaimSkipsBelowMemoryThreshold(): void {
  const decision = shouldReclaim(
    makeSettings({ minResidentMB: 400, reclaimOnGhostWindows: false }),
    makeSnapshot({ residentBytes: 399 * BYTES_PER_MB }),
    makeStats(),
    1_000_000,
  );
  assert(!decision, "below the resident threshold nothing should happen");
}

function testShouldReclaimRunsAtExactThreshold(): void {
  const decision = shouldReclaim(
    makeSettings({ minResidentMB: 400, reclaimOnGhostWindows: false }),
    makeSnapshot({ residentBytes: 400 * BYTES_PER_MB }),
    makeStats(),
    1_000_000,
  );
  assert(decision, "the threshold itself should be inclusive");
}

function testShouldReclaimRunsOnGhostWindows(): void {
  const decision = shouldReclaim(
    makeSettings({ minResidentMB: 4_000, reclaimOnGhostWindows: true }),
    // Resident memory is far below the floor, but ghost windows exist.
    makeSnapshot({ residentBytes: 10 * BYTES_PER_MB, ghostWindows: 2 }),
    makeStats(),
    1_000_000,
  );
  assert(decision, "ghost windows should trigger reclaim regardless of size");
}

function testShouldReclaimIgnoresGhostWindowsWhenDisabled(): void {
  const decision = shouldReclaim(
    makeSettings({ minResidentMB: 4_000, reclaimOnGhostWindows: false }),
    makeSnapshot({ residentBytes: 10 * BYTES_PER_MB, ghostWindows: 2 }),
    makeStats(),
    1_000_000,
  );
  assert(
    !decision,
    "ghost window trigger must be honoured only when enabled",
  );
}

function testShouldReclaimThrottleBeatsGhostWindows(): void {
  const now = 1_000_000;
  const decision = shouldReclaim(
    makeSettings({ minIntervalSec: 300, reclaimOnGhostWindows: true }),
    makeSnapshot({ ghostWindows: 5 }),
    makeStats({ lastRunAt: now - 1_000 }),
    now,
  );
  assert(!decision, "throttle must apply even when ghost windows exist");
}

function testShouldReclaimSurvivesClockRollback(): void {
  // A backwards system clock leaves lastRunAt in the future. Blocking forever
  // there would kill the feature, so the reclaim has to be allowed through.
  const now = 1_000_000;
  const decision = shouldReclaim(
    makeSettings({ minResidentMB: 0 }),
    makeSnapshot({ residentBytes: 500 * BYTES_PER_MB }),
    makeStats({ lastRunAt: now + 999_999_999 }),
    now,
  );
  assert(decision, "a future lastRunAt must not block reclaim forever");
}

// ---------------------------------------------------------------------------
// isIdleEnough
// ---------------------------------------------------------------------------

function testIsIdleEnoughBelowThreshold(): void {
  const settings = makeSettings({ idleThresholdSec: 60 });
  assert(
    !isIdleEnough(59_999, settings),
    "just under the threshold must not count as idle",
  );
}

function testIsIdleEnoughAtThreshold(): void {
  const settings = makeSettings({ idleThresholdSec: 60 });
  assert(isIdleEnough(60_000, settings), "the threshold itself is inclusive");
}

function testIsIdleEnoughAfterUserReturns(): void {
  // The user coming back resets idleTime to roughly zero. Gathering the memory
  // snapshot is asynchronous, so this is re-checked before the reclaim starts.
  const settings = makeSettings({ idleThresholdSec: 60 });
  assert(
    !isIdleEnough(0, settings),
    "a returning user must cancel the pending reclaim",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    // SANITIZE SETTINGS
    { name: "sanitizeSettings with null", fn: testSanitizeSettingsWithNull },
    {
      name: "sanitizeSettings with non-object",
      fn: testSanitizeSettingsWithNonObject,
    },
    {
      name: "sanitizeSettings with partial object",
      fn: testSanitizeSettingsWithPartialObject,
    },
    {
      name: "sanitizeSettings rejects wrong types",
      fn: testSanitizeSettingsRejectsWrongTypes,
    },
    {
      name: "sanitizeSettings clamps idle threshold",
      fn: testSanitizeSettingsClampsIdleThreshold,
    },
    {
      name: "sanitizeSettings clamps reclaim interval",
      fn: testSanitizeSettingsClampsInterval,
    },
    {
      name: "sanitizeSettings clamps poll interval",
      fn: testSanitizeSettingsClampsPollInterval,
    },
    {
      name: "sanitizeSettings rejects NaN and Infinity",
      fn: testSanitizeSettingsRejectsNonFinite,
    },
    {
      name: "sanitizeSettings accepts valid values",
      fn: testSanitizeSettingsAcceptsValidValues,
    },
    // SANITIZE STATS
    { name: "sanitizeStats with null", fn: testSanitizeStatsWithNull },
    {
      name: "sanitizeStats preserves negative lastFreedBytes",
      fn: testSanitizeStatsPreservesNegativeFreed,
    },
    {
      name: "sanitizeStats clamps negative counters",
      fn: testSanitizeStatsClampsNegativeCounters,
    },
    // SHOULD RECLAIM
    {
      name: "shouldReclaim skips when disabled",
      fn: testShouldReclaimSkipsWhenDisabled,
    },
    {
      name: "shouldReclaim skips when throttled",
      fn: testShouldReclaimSkipsWhenThrottled,
    },
    {
      name: "shouldReclaim runs after throttle expires",
      fn: testShouldReclaimRunsAfterThrottleExpires,
    },
    {
      name: "shouldReclaim skips below memory threshold",
      fn: testShouldReclaimSkipsBelowMemoryThreshold,
    },
    {
      name: "shouldReclaim runs at exact threshold",
      fn: testShouldReclaimRunsAtExactThreshold,
    },
    {
      name: "shouldReclaim runs on ghost windows",
      fn: testShouldReclaimRunsOnGhostWindows,
    },
    {
      name: "shouldReclaim ignores ghost windows when disabled",
      fn: testShouldReclaimIgnoresGhostWindowsWhenDisabled,
    },
    {
      name: "shouldReclaim throttle beats ghost windows",
      fn: testShouldReclaimThrottleBeatsGhostWindows,
    },
    {
      name: "isIdleEnough below threshold",
      fn: testIsIdleEnoughBelowThreshold,
    },
    {
      name: "isIdleEnough at exact threshold",
      fn: testIsIdleEnoughAtThreshold,
    },
    {
      name: "isIdleEnough after the user returns",
      fn: testIsIdleEnoughAfterUserReturns,
    },
    {
      name: "shouldReclaim survives clock rollback",
      fn: testShouldReclaimSurvivesClockRollback,
    },
  ];

  await runTests("idleMemoryReclaim.test.ts", tests);
}
