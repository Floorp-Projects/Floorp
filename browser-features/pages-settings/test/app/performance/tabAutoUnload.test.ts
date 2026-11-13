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
  DEFAULT_TAB_AUTO_UNLOAD_SETTINGS,
  getTabAutoUnloadSettings,
  LEGACY_TAB_SLEEP_EXCLUSION_PREF,
  MEMORY_THRESHOLD_MB_MIN,
  mergeTabAutoUnloadSettings,
  MIN_INACTIVE_MINUTES_MIN,
  normalizeTabAutoUnloadSettings,
  saveTabAutoUnloadSettings,
  TAB_AUTO_UNLOAD_PREF,
  type TabAutoUnloadSettings,
} from "../../../src/app/performance/dataManager.ts";

declare const Services: {
  prefs: {
    clearUserPref(name: string): void;
    getStringPref(name: string, fallback: string): string;
    setStringPref(name: string, value: string): void;
  };
};

/** Builds settings for a test with the given overrides. */
function makeSettings(
  overrides: Partial<TabAutoUnloadSettings> = {},
): TabAutoUnloadSettings {
  return {
    ...DEFAULT_TAB_AUTO_UNLOAD_SETTINGS,
    ...overrides,
    excludePatterns: overrides.excludePatterns ?? [],
  };
}

/** Clears a pref, ignoring one that is already unset. */
function clearPref(name: string): void {
  try {
    Services.prefs.clearUserPref(name);
  } catch {
    // Already unset.
  }
}

/** Defaults are opt-in and the floors match the feature. */
function testDefaults(): void {
  assertEquals(
    DEFAULT_TAB_AUTO_UNLOAD_SETTINGS.enabled,
    false,
    "default must be opt-in",
  );
  assertEquals(
    DEFAULT_TAB_AUTO_UNLOAD_SETTINGS.memoryThresholdMB,
    2048,
    "default memory threshold",
  );
  assertEquals(
    DEFAULT_TAB_AUTO_UNLOAD_SETTINGS.minInactiveMinutes,
    10,
    "default inactive time",
  );
  assertEquals(
    DEFAULT_TAB_AUTO_UNLOAD_SETTINGS.excludePatterns.length,
    0,
    "default exclusions",
  );
}

/** Non-object pref JSON falls back to the defaults. */
function testNormalizeFallsBackForNonObjects(): void {
  const result = normalizeTabAutoUnloadSettings("nope");
  assertEquals(result.enabled, false, "enabled fallback");
  assertEquals(result.memoryThresholdMB, 2048, "threshold fallback");
  assertEquals(result.minInactiveMinutes, 10, "inactive fallback");
  assertEquals(result.excludePatterns.length, 0, "patterns fallback");
}

/** Wrong types fall back and numeric floors are clamped. */
function testNormalizeRejectsWrongTypesAndClamps(): void {
  const result = normalizeTabAutoUnloadSettings({
    enabled: "yes",
    memoryThresholdMB: "1024",
    minInactiveMinutes: 0,
    excludePatterns: ["  example.com  ", "", "example.com", 42, "*.test"],
  });

  assertEquals(result.enabled, false, "wrong bool type");
  assertEquals(result.memoryThresholdMB, 2048, "wrong number type");
  assertEquals(
    result.minInactiveMinutes,
    MIN_INACTIVE_MINUTES_MIN,
    "inactive time must be clamped to the floor",
  );
  assertEquals(
    result.excludePatterns.join(","),
    "example.com,*.test",
    "patterns must be trimmed, deduplicated, and type-checked",
  );
}

/** Memory thresholds below the floor are clamped. */
function testNormalizeClampsMemoryFloor(): void {
  const result = normalizeTabAutoUnloadSettings({ memoryThresholdMB: -10 });
  assertEquals(
    result.memoryThresholdMB,
    MEMORY_THRESHOLD_MB_MIN,
    "memory threshold must be clamped",
  );
}

/** Merging keeps keys this page does not expose. */
function testMergeKeepsUnknownKeys(): void {
  const existing = JSON.stringify({
    enabled: true,
    memoryThresholdMB: 1024,
    minInactiveMinutes: 5,
    excludePatterns: ["old.example"],
    handEditedKey: "keep-me",
  });
  const merged = JSON.parse(
    mergeTabAutoUnloadSettings(existing, makeSettings({ enabled: false })),
  ) as Record<string, unknown>;

  assertEquals(
    merged.handEditedKey,
    "keep-me",
    "unknown keys must survive a save",
  );
  assertEquals(merged.enabled, false, "exposed keys must be replaced");
  assertEquals(
    merged.excludePatterns instanceof Array &&
      (merged.excludePatterns as unknown[]).length,
    0,
    "exposed patterns must be replaced",
  );
}

/** Merging tolerates malformed stored JSON. */
function testMergeFromBrokenJson(): void {
  const merged = JSON.parse(
    mergeTabAutoUnloadSettings("{ bad json", makeSettings()),
  ) as Record<string, unknown>;
  assertEquals(
    merged.memoryThresholdMB,
    DEFAULT_TAB_AUTO_UNLOAD_SETTINGS.memoryThresholdMB,
    "broken JSON should fall back to defaults",
  );
}

/** The legacy exclusion pref supplies patterns without a write. */
async function testLegacyFallback(): Promise<void> {
  const originalNew = Services.prefs.getStringPref(TAB_AUTO_UNLOAD_PREF, "");
  const originalLegacy = Services.prefs.getStringPref(
    LEGACY_TAB_SLEEP_EXCLUSION_PREF,
    "",
  );

  try {
    clearPref(TAB_AUTO_UNLOAD_PREF);
    Services.prefs.setStringPref(
      LEGACY_TAB_SLEEP_EXCLUSION_PREF,
      JSON.stringify({ enabled: true, patterns: ["legacy.example"] }),
    );

    const loaded = await getTabAutoUnloadSettings();
    assertEquals(
      loaded.excludePatterns.join(","),
      "legacy.example",
      "legacy patterns should be used when the new pref is absent",
    );
    assertEquals(loaded.enabled, false, "legacy enabled state is not migrated");
  } finally {
    if (originalNew) {
      Services.prefs.setStringPref(TAB_AUTO_UNLOAD_PREF, originalNew);
    } else {
      clearPref(TAB_AUTO_UNLOAD_PREF);
    }
    if (originalLegacy) {
      Services.prefs.setStringPref(
        LEGACY_TAB_SLEEP_EXCLUSION_PREF,
        originalLegacy,
      );
    } else {
      clearPref(LEGACY_TAB_SLEEP_EXCLUSION_PREF);
    }
  }
}

async function testDisabledLegacyFallback(): Promise<void> {
  const originalNew = Services.prefs.getStringPref(TAB_AUTO_UNLOAD_PREF, "");
  const originalLegacy = Services.prefs.getStringPref(
    LEGACY_TAB_SLEEP_EXCLUSION_PREF,
    "",
  );

  try {
    clearPref(TAB_AUTO_UNLOAD_PREF);
    Services.prefs.setStringPref(
      LEGACY_TAB_SLEEP_EXCLUSION_PREF,
      JSON.stringify({ enabled: false, patterns: ["legacy.example"] }),
    );

    const loaded = await getTabAutoUnloadSettings();
    assertEquals(
      loaded.excludePatterns.length,
      0,
      "disabled legacy patterns must not become active exclusions",
    );
  } finally {
    if (originalNew) {
      Services.prefs.setStringPref(TAB_AUTO_UNLOAD_PREF, originalNew);
    } else {
      clearPref(TAB_AUTO_UNLOAD_PREF);
    }
    if (originalLegacy) {
      Services.prefs.setStringPref(
        LEGACY_TAB_SLEEP_EXCLUSION_PREF,
        originalLegacy,
      );
    } else {
      clearPref(LEGACY_TAB_SLEEP_EXCLUSION_PREF);
    }
  }
}

/** Saved settings are read back unchanged. */
async function testSaveRoundTrip(): Promise<void> {
  const original = Services.prefs.getStringPref(TAB_AUTO_UNLOAD_PREF, "");

  try {
    clearPref(TAB_AUTO_UNLOAD_PREF);
    await saveTabAutoUnloadSettings(
      makeSettings({
        enabled: true,
        memoryThresholdMB: 4096,
        minInactiveMinutes: 20,
        excludePatterns: ["*.example.com"],
      }),
    );

    const loaded = await getTabAutoUnloadSettings();
    assertEquals(loaded.enabled, true, "enabled should round-trip");
    assertEquals(
      loaded.memoryThresholdMB,
      4096,
      "memory threshold should round-trip",
    );
    assertEquals(
      loaded.minInactiveMinutes,
      20,
      "inactive time should round-trip",
    );
    assertEquals(
      loaded.excludePatterns.join(","),
      "*.example.com",
      "patterns should round-trip",
    );
  } finally {
    if (original) {
      Services.prefs.setStringPref(TAB_AUTO_UNLOAD_PREF, original);
    } else {
      clearPref(TAB_AUTO_UNLOAD_PREF);
    }
  }
}

/** en-US and ja-JP expose the same non-empty keys. */
function testTranslationsExist(): void {
  const enKeys = Object.keys(enUS.performance.tabAutoUnload).sort();
  const jaKeys = Object.keys(jaJP.performance.tabAutoUnload).sort();
  assertEquals(
    enKeys.join(","),
    jaKeys.join(","),
    "en-US and ja-JP must expose the same tab auto unload keys",
  );

  for (const key of enKeys) {
    const english = enUS.performance.tabAutoUnload[
      key as keyof typeof enUS.performance.tabAutoUnload
    ];
    const japanese = jaJP.performance.tabAutoUnload[
      key as keyof typeof jaJP.performance.tabAutoUnload
    ];
    assert(
      typeof english === "string" && english.trim().length > 0,
      `en-US performance.tabAutoUnload.${key} must be non-empty`,
    );
    assert(
      typeof japanese === "string" && japanese.trim().length > 0,
      `ja-JP performance.tabAutoUnload.${key} must be non-empty`,
    );
  }
}

/** Runs every settings-side tab auto unload test. */
export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "tab auto unload defaults", fn: testDefaults },
    {
      name: "normalization falls back for non-objects",
      fn: testNormalizeFallsBackForNonObjects,
    },
    {
      name: "normalization rejects wrong types and clamps",
      fn: testNormalizeRejectsWrongTypesAndClamps,
    },
    {
      name: "normalization clamps the memory floor",
      fn: testNormalizeClampsMemoryFloor,
    },
    {
      name: "merge keeps unknown keys",
      fn: testMergeKeepsUnknownKeys,
    },
    {
      name: "merge recovers from broken JSON",
      fn: testMergeFromBrokenJson,
    },
    {
      name: "legacy pref supplies patterns",
      fn: testLegacyFallback,
    },
    {
      name: "disabled legacy pref does not supply patterns",
      fn: testDisabledLegacyFallback,
    },
    {
      name: "settings save and reload",
      fn: testSaveRoundTrip,
    },
    {
      name: "tab auto unload translations exist",
      fn: testTranslationsExist,
    },
  ];

  await runTests("tabAutoUnload.test.ts", tests);
}
