// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import enUS from "../../../src/lib/i18n/locales/en-US.json" with {
  type: "json",
};
import jaJP from "../../../src/lib/i18n/locales/ja-JP.json" with {
  type: "json",
};
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS,
  IDLE_THRESHOLD_SEC_MIN,
  type IdleMemoryReclaimSettings,
  mergeIdleMemoryReclaimSettings,
  MIN_INTERVAL_SEC_MIN,
  normalizeIdleMemoryReclaimSettings,
} from "../../../src/app/performance/dataManager.ts";

const FULL_PREF_JSON = JSON.stringify({
  enabled: true,
  idleThresholdSec: 60,
  pollIntervalSec: 60,
  minIntervalSec: 300,
  minResidentMB: 400,
  reclaimOnGhostWindows: true,
});

function makeSettings(
  overrides: Partial<IdleMemoryReclaimSettings> = {},
): IdleMemoryReclaimSettings {
  return { ...DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS, ...overrides };
}

// The shared harness compares with ===, so object results are asserted field by
// field rather than as a whole.
function assertDefaults(
  settings: IdleMemoryReclaimSettings,
  context: string,
): void {
  assertEquals(
    settings.enabled,
    DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS.enabled,
    `${context}: enabled`,
  );
  assertEquals(
    settings.idleThresholdSec,
    DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS.idleThresholdSec,
    `${context}: idleThresholdSec`,
  );
  assertEquals(
    settings.minIntervalSec,
    DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS.minIntervalSec,
    `${context}: minIntervalSec`,
  );
  assertEquals(
    settings.minResidentMB,
    DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS.minResidentMB,
    `${context}: minResidentMB`,
  );
}

function testNormalizeFallsBackForNonObjects(): void {
  assertDefaults(normalizeIdleMemoryReclaimSettings(null), "null");
  assertDefaults(normalizeIdleMemoryReclaimSettings("nope"), "a non-object");
}

function testNormalizeRejectsWrongTypes(): void {
  const result = normalizeIdleMemoryReclaimSettings({
    enabled: "yes",
    idleThresholdSec: "600",
    minIntervalSec: null,
    minResidentMB: {},
  });
  assertEquals(
    result.enabled,
    DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS.enabled,
    "a string should not be accepted as a boolean",
  );
  assertEquals(
    result.idleThresholdSec,
    DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS.idleThresholdSec,
    "a string should not be accepted as a number",
  );
  assertEquals(
    result.minIntervalSec,
    DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS.minIntervalSec,
    "null should not be accepted as a number",
  );
  assertEquals(
    result.minResidentMB,
    DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS.minResidentMB,
    "an object should not be accepted as a number",
  );
}

function testNormalizeClampsBelowPageFloors(): void {
  const result = normalizeIdleMemoryReclaimSettings({
    idleThresholdSec: 15,
    minIntervalSec: 30,
  });
  assertEquals(
    result.idleThresholdSec,
    IDLE_THRESHOLD_SEC_MIN,
    "the idle threshold should be clamped to the page floor",
  );
  assertEquals(
    result.minIntervalSec,
    MIN_INTERVAL_SEC_MIN,
    "the reclaim interval should be clamped to the page floor",
  );
}

function testNormalizeRejectsNonFinite(): void {
  const result = normalizeIdleMemoryReclaimSettings({
    idleThresholdSec: Number.NaN,
    minIntervalSec: Number.POSITIVE_INFINITY,
  });
  assertEquals(
    result.idleThresholdSec,
    DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS.idleThresholdSec,
    "NaN should fall back to the default",
  );
  assertEquals(
    result.minIntervalSec,
    DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS.minIntervalSec,
    "Infinity should fall back to the default",
  );
}

function testNormalizeAcceptsValidValues(): void {
  const result = normalizeIdleMemoryReclaimSettings({
    enabled: false,
    idleThresholdSec: 120,
    minIntervalSec: 600,
    minResidentMB: 0,
  });
  assertEquals(result.enabled, false, "enabled should round-trip");
  assertEquals(result.idleThresholdSec, 120, "idleThresholdSec should pass");
  assertEquals(result.minIntervalSec, 600, "minIntervalSec should pass");
  assertEquals(result.minResidentMB, 0, "0 is a valid memory floor");
}

function testMergeKeepsFieldsThePageDoesNotExpose(): void {
  const merged = JSON.parse(
    mergeIdleMemoryReclaimSettings(FULL_PREF_JSON, makeSettings()),
  ) as Record<string, unknown>;

  assertEquals(
    merged.pollIntervalSec,
    60,
    "pollIntervalSec is not shown on the page and must survive a save",
  );
  assertEquals(
    merged.reclaimOnGhostWindows,
    true,
    "reclaimOnGhostWindows is not shown on the page and must survive a save",
  );
}

function testMergeOverridesExposedFields(): void {
  const merged = JSON.parse(
    mergeIdleMemoryReclaimSettings(
      FULL_PREF_JSON,
      makeSettings({
        enabled: false,
        idleThresholdSec: 900,
        minIntervalSec: 1200,
        minResidentMB: 1024,
      }),
    ),
  ) as Record<string, unknown>;

  assertEquals(merged.enabled, false, "enabled should be updated");
  assertEquals(
    merged.idleThresholdSec,
    900,
    "idleThresholdSec should be updated",
  );
  assertEquals(merged.minIntervalSec, 1200, "minIntervalSec should be updated");
  assertEquals(merged.minResidentMB, 1024, "minResidentMB should be updated");
  assertEquals(
    merged.pollIntervalSec,
    60,
    "exposing four fields must not drop the others",
  );
}

function testMergeStartsFromDefaultsWhenPrefIsMissing(): void {
  const merged = JSON.parse(
    mergeIdleMemoryReclaimSettings(null, makeSettings()),
  ) as Record<string, unknown>;

  assertEquals(merged.enabled, true, "a missing pref should be filled in");
  assertEquals(
    merged.idleThresholdSec,
    DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS.idleThresholdSec,
    "a missing pref should use the default idle threshold",
  );
  assert(
    !("pollIntervalSec" in merged),
    "a missing pref has no hidden keys to preserve",
  );
}

function testMergeRecoversFromBrokenPrefJson(): void {
  const merged = JSON.parse(
    mergeIdleMemoryReclaimSettings("{ not json", makeSettings()),
  ) as Record<string, unknown>;

  assertEquals(
    merged.minIntervalSec,
    DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS.minIntervalSec,
    "unparseable pref JSON should fall back to defaults",
  );
}

function testMergeClampsValuesBeforeWriting(): void {
  const merged = JSON.parse(
    mergeIdleMemoryReclaimSettings(
      FULL_PREF_JSON,
      makeSettings({ idleThresholdSec: 1 }),
    ),
  ) as Record<string, unknown>;

  assertEquals(
    merged.idleThresholdSec,
    IDLE_THRESHOLD_SEC_MIN,
    "a save must not write a value below the page floor",
  );
}

function testPerformanceTranslationsExist(): void {
  assertEquals(
    enUS.pages.performance,
    "Performance",
    "English page title should exist",
  );
  assertEquals(
    jaJP.pages.performance,
    "パフォーマンス",
    "Japanese page title should exist",
  );
  assertEquals(
    enUS.performance.idleReclaim.title,
    "Idle memory reclaim",
    "English card title should exist",
  );
  assertEquals(
    jaJP.performance.idleReclaim.title,
    "アイドル時のメモリ回収",
    "Japanese card title should exist",
  );

  const enKeys = Object.keys(enUS.performance.idleReclaim).sort();
  const jaKeys = Object.keys(jaJP.performance.idleReclaim).sort();
  assertEquals(
    enKeys.join(","),
    jaKeys.join(","),
    "en-US and ja-JP must expose the same idle reclaim keys",
  );
  for (const key of enKeys) {
    const english =
      enUS.performance.idleReclaim[key as keyof typeof enUS.performance
        .idleReclaim];
    const japanese =
      jaJP.performance.idleReclaim[key as keyof typeof jaJP.performance
        .idleReclaim];
    assert(
      typeof english === "string" && english.trim().length > 0,
      `en-US performance.idleReclaim.${key} must be non-empty`,
    );
    assert(
      typeof japanese === "string" && japanese.trim().length > 0,
      `ja-JP performance.idleReclaim.${key} must be non-empty`,
    );
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "idle reclaim normalization falls back for non-objects",
      fn: testNormalizeFallsBackForNonObjects,
    },
    {
      name: "idle reclaim normalization rejects wrong types",
      fn: testNormalizeRejectsWrongTypes,
    },
    {
      name: "idle reclaim normalization clamps below the page floors",
      fn: testNormalizeClampsBelowPageFloors,
    },
    {
      name: "idle reclaim normalization rejects NaN and Infinity",
      fn: testNormalizeRejectsNonFinite,
    },
    {
      name: "idle reclaim normalization accepts valid values",
      fn: testNormalizeAcceptsValidValues,
    },
    {
      name: "idle reclaim merge keeps fields the page does not expose",
      fn: testMergeKeepsFieldsThePageDoesNotExpose,
    },
    {
      name: "idle reclaim merge overrides exposed fields",
      fn: testMergeOverridesExposedFields,
    },
    {
      name: "idle reclaim merge starts from defaults when the pref is missing",
      fn: testMergeStartsFromDefaultsWhenPrefIsMissing,
    },
    {
      name: "idle reclaim merge recovers from broken pref JSON",
      fn: testMergeRecoversFromBrokenPrefJson,
    },
    {
      name: "idle reclaim merge clamps values before writing",
      fn: testMergeClampsValuesBeforeWriting,
    },
    {
      name: "performance translations exist",
      fn: testPerformanceTranslationsExist,
    },
  ];

  await runTests("idleMemoryReclaim.test.ts", tests);
}
