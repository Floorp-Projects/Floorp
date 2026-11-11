// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  DEFAULT_SETTINGS,
  DEFAULT_STATS,
  LEGACY_TAB_SLEEP_EXCLUSION_PREF,
  MIN_CHECK_INTERVAL_MS,
  MIN_INACTIVE_MINUTES,
  TAB_AUTO_UNLOAD_PREF,
  TAB_AUTO_UNLOAD_STATS_PREF,
  type TabAutoUnloadSettings,
} from "../types.ts";
import {
  legacyPatterns,
  matchPattern,
  normalizeExcludePatterns,
  sanitizeSettings,
  sanitizeStats,
  shouldCheckMemory,
  shouldExcludeUrl,
} from "../index.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

/** Builds settings for a test with the given overrides. */
function makeSettings(
  overrides: Partial<TabAutoUnloadSettings> = {},
): TabAutoUnloadSettings {
  return { ...DEFAULT_SETTINGS, ...overrides, excludePatterns: [] };
}

/** Defaults stay opt-in and the pref names stay stable. */
function testDefaultSettingsContract(): void {
  assertEquals(DEFAULT_SETTINGS.enabled, false, "default must be opt-in");
  assertEquals(
    DEFAULT_SETTINGS.memoryThresholdMB,
    2048,
    "default memory threshold",
  );
  assertEquals(
    DEFAULT_SETTINGS.minInactiveMinutes,
    10,
    "default inactive time",
  );
  assertEquals(
    DEFAULT_SETTINGS.excludePatterns.length,
    0,
    "default exclusions",
  );
  assertEquals(
    TAB_AUTO_UNLOAD_PREF,
    "floorp.tabs.autoUnload",
    "pref name should stay stable",
  );
  assertEquals(
    TAB_AUTO_UNLOAD_STATS_PREF,
    "floorp.tabs.autoUnload.stats",
    "stats pref name should stay stable",
  );
  assertEquals(
    LEGACY_TAB_SLEEP_EXCLUSION_PREF,
    "floorp.tabs.sleep.exclusion",
    "legacy pref name should stay stable",
  );
}

/** Non-object pref JSON falls back to the defaults. */
function testSanitizeFallsBackForNonObjects(): void {
  const result = sanitizeSettings("nope");
  assertEquals(result.enabled, DEFAULT_SETTINGS.enabled, "enabled fallback");
  assertEquals(
    result.memoryThresholdMB,
    DEFAULT_SETTINGS.memoryThresholdMB,
    "threshold fallback",
  );
  assertEquals(
    result.minInactiveMinutes,
    DEFAULT_SETTINGS.minInactiveMinutes,
    "inactive fallback",
  );
  assertEquals(result.excludePatterns.length, 0, "patterns fallback");
}

/** Wrong types fall back and numeric floors are clamped. */
function testSanitizeRejectsWrongTypesAndClamps(): void {
  const result = sanitizeSettings({
    enabled: "yes",
    memoryThresholdMB: "1024",
    minInactiveMinutes: 0,
    excludePatterns: ["  example.com  ", "", "example.com", 42, "*.test"],
  });

  assertEquals(result.enabled, DEFAULT_SETTINGS.enabled, "wrong bool type");
  assertEquals(
    result.memoryThresholdMB,
    DEFAULT_SETTINGS.memoryThresholdMB,
    "wrong number type",
  );
  assertEquals(
    result.minInactiveMinutes,
    MIN_INACTIVE_MINUTES,
    "inactive time must be clamped to the floor",
  );
  assertEquals(
    result.excludePatterns.join(","),
    "example.com,*.test",
    "patterns must be trimmed, deduplicated, and type-checked",
  );
}

/** Valid pref JSON round-trips unchanged. */
function testSanitizeAcceptsValidValues(): void {
  const result = sanitizeSettings({
    enabled: true,
    memoryThresholdMB: 512,
    minInactiveMinutes: 30,
    excludePatterns: ["mail.example.com"],
  });
  assertEquals(result.enabled, true, "enabled should round-trip");
  assertEquals(result.memoryThresholdMB, 512, "threshold should round-trip");
  assertEquals(result.minInactiveMinutes, 30, "inactive should round-trip");
  assertEquals(
    result.excludePatterns[0],
    "mail.example.com",
    "patterns should round-trip",
  );
}

/** Stats keep valid numbers and fall back otherwise. */
function testSanitizeStats(): void {
  const result = sanitizeStats({
    lastRunAt: 100,
    lastUnloadAt: 50,
    unloadCount: 3,
  });
  assertEquals(result.lastRunAt, 100, "lastRunAt");
  assertEquals(result.lastUnloadAt, 50, "lastUnloadAt");
  assertEquals(result.unloadCount, 3, "unloadCount");

  const fallback = sanitizeStats({ lastRunAt: -1, unloadCount: "x" });
  assertEquals(fallback.lastRunAt, 0, "negative timestamps are clamped");
  assertEquals(fallback.unloadCount, 0, "wrong types fall back");
}

/** Patterns are trimmed, deduplicated, and type-checked. */
function testNormalizeExcludePatterns(): void {
  assertEquals(
    normalizeExcludePatterns([" a ", "a", "", "b"]).join(","),
    "a,b",
    "patterns should be trimmed and deduplicated",
  );
  assertEquals(
    normalizeExcludePatterns("nope").length,
    0,
    "non-arrays should be empty",
  );
}

/** Bare and wildcard domains match the domain and its subdomains. */
function testMatchDomainAndSubdomain(): void {
  assert(
    matchPattern("https://example.com/path", "example.com"),
    "bare domain should match",
  );
  assert(
    matchPattern("https://sub.example.com/path", "example.com"),
    "bare domain should match subdomains",
  );
  assert(
    matchPattern("https://sub.example.com/path", "*.example.com"),
    "wildcard should match subdomains",
  );
  assert(
    matchPattern("https://example.com/path", "*.example.com"),
    "wildcard should match the apex domain too",
  );
  assert(
    !matchPattern("https://notexample.com/", "example.com"),
    "domain matching must not match a suffix of another host",
  );
  assert(
    !matchPattern("https://mozilla.org/", "example.com"),
    "unrelated domains should not match",
  );
  assert(
    matchPattern("https://EXAMPLE.com/", "example.com"),
    "matching should be case-insensitive",
  );
}

/** Full URL patterns support prefixes, wildcards, and literal symbols. */
function testMatchFullUrl(): void {
  assert(
    matchPattern("https://mail.example.com/inbox", "https://mail.example.com"),
    "a full URL without a wildcard should act as a prefix",
  );
  assert(
    matchPattern(
      "https://mail.example.com/inbox?q=1",
      "https://mail.example.com/*",
    ),
    "a full URL wildcard should match a path",
  );
  assert(
    !matchPattern("https://example.com/", "https://mail.example.com/*"),
    "a different host should not match",
  );
  assert(
    matchPattern("https://example.com/a+b", "https://example.com/a+b"),
    "regex special characters must be treated literally",
  );
}

/** An empty pattern matches nothing. */
function testMatchEmptyPattern(): void {
  assert(
    !matchPattern("https://example.com/", "   "),
    "an empty pattern must not match everything",
  );
}

/** A URL is excluded when any pattern matches; an empty list excludes none. */
function testShouldExcludeUrl(): void {
  assert(
    shouldExcludeUrl("https://docs.example.com/a", ["*.example.com"]),
    "any matching pattern should exclude",
  );
  assert(
    !shouldExcludeUrl("https://mozilla.org/", ["*.example.com"]),
    "non-matching URL should not be excluded",
  );
  assert(
    !shouldExcludeUrl("https://example.com/", []),
    "an empty list excludes nothing",
  );
}

/** Legacy exclusion prefs are parsed defensively. */
function testLegacyPatterns(): void {
  assertEquals(
    legacyPatterns('{"enabled":true,"patterns":["a.com"," a.com ","b.com"]}')
      .join(","),
    "a.com,b.com",
    "legacy patterns should be normalized",
  );
  assertEquals(legacyPatterns("").length, 0, "empty legacy pref");
  assertEquals(legacyPatterns("{bad json").length, 0, "broken legacy pref");
  assertEquals(
    legacyPatterns('{"patterns":"nope"}').length,
    0,
    "non-array legacy patterns",
  );
}

/** The memory threshold and cross-window throttle gate checks. */
function testShouldCheckMemory(): void {
  const settings = makeSettings({
    enabled: true,
    memoryThresholdMB: 1024,
  });
  const stats = { ...DEFAULT_STATS, lastRunAt: 0 };
  const now = 10_000_000;

  assert(
    !shouldCheckMemory(
      { ...settings, enabled: false },
      2048 * 1024 * 1024,
      stats,
      now,
    ),
    "disabled settings must not run",
  );
  assert(
    !shouldCheckMemory(settings, 1023 * 1024 * 1024, stats, now),
    "memory below the threshold must not unload",
  );
  assert(
    shouldCheckMemory(settings, 1024 * 1024 * 1024, stats, now),
    "memory at the threshold should unload",
  );
  assert(
    !shouldCheckMemory(settings, 4096 * 1024 * 1024, {
      ...stats,
      lastRunAt: now - 1000,
    }, now),
    "the cross-window throttle must suppress a second check",
  );
  assert(
    shouldCheckMemory(settings, 4096 * 1024 * 1024, {
      ...stats,
      lastRunAt: now - MIN_CHECK_INTERVAL_MS,
    }, now),
    "a check after the interval should run",
  );
}

/** Runs every tab auto unload test. */
export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "tab auto unload default settings contract",
      fn: testDefaultSettingsContract,
    },
    {
      name: "settings fall back for non-objects",
      fn: testSanitizeFallsBackForNonObjects,
    },
    {
      name: "settings reject wrong types and clamp floors",
      fn: testSanitizeRejectsWrongTypesAndClamps,
    },
    {
      name: "settings accept valid values",
      fn: testSanitizeAcceptsValidValues,
    },
    {
      name: "stats are sanitized",
      fn: testSanitizeStats,
    },
    {
      name: "exclude patterns are normalized",
      fn: testNormalizeExcludePatterns,
    },
    {
      name: "domain patterns match domains and subdomains",
      fn: testMatchDomainAndSubdomain,
    },
    {
      name: "full URL patterns match",
      fn: testMatchFullUrl,
    },
    {
      name: "empty patterns match nothing",
      fn: testMatchEmptyPattern,
    },
    {
      name: "shouldExcludeUrl handles lists",
      fn: testShouldExcludeUrl,
    },
    {
      name: "legacy patterns are read safely",
      fn: testLegacyPatterns,
    },
    {
      name: "memory checks honor threshold and throttle",
      fn: testShouldCheckMemory,
    },
  ];

  await runTests("tabAutoUnload.test.ts", tests);
}
