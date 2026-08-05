// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertStringArrayEqual,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  DEFAULT_CATEGORY_PRIORITY,
  getCommandPaletteSettings,
  saveCommandPaletteSettings,
} from "../../../src/app/command-palette/dataManager.ts";

/**
 * Unit tests for command-palette dataManager.
 *
 * These tests run inside the actual browser (colocated-env browser), so
 * `rpc.getBoolPref` / `rpc.setBoolPref` resolve to `directServicesFunctions`
 * in `src/lib/rpc/rpc.ts`, which delegate to `Services.prefs` directly.
 * We therefore drive `Services.prefs` to set up known state, invoke the
 * dataManager functions, and assert the observed behavior.
 *
 * The pref `floorp.commandPalette.enabled` has no default value defined in
 * the pref `.ini` files, so `clearUserPref` reliably makes `getPrefType`
 * return PREF_INVALID, which causes `rpc.getBoolPref` to resolve to `null`.
 */

const PREF = "floorp.commandPalette.enabled";
const CATEGORY_PRIORITY_PREF = "floorp.commandPalette.categoryPriority";

// `Services` is a Firefox global available in the browser test environment.
// deno-lint-ignore no-explicit-any
declare const Services: any;

/** Read the raw pref value as a boolean, or return null when not set. */
function readRawPref(): boolean | null {
  if (Services.prefs.getPrefType(PREF) !== Services.prefs.PREF_BOOL) {
    return null;
  }
  return Services.prefs.getBoolPref(PREF);
}

/** True when the pref is currently unset (PREF_INVALID). */
function isPrefUnset(): boolean {
  return Services.prefs.getPrefType(PREF) === Services.prefs.PREF_INVALID;
}

/** Reads the raw string value of the category-priority pref, or null when unset. */
function readRawCategoryPriorityPref(): string | null {
  if (
    Services.prefs.getPrefType(CATEGORY_PRIORITY_PREF) !==
    Services.prefs.PREF_STRING
  ) {
    return null;
  }
  return Services.prefs.getStringPref(CATEGORY_PRIORITY_PREF);
}

/** True when the category-priority pref is currently unset (PREF_INVALID). */
function isCategoryPriorityPrefUnset(): boolean {
  return Services.prefs.getPrefType(CATEGORY_PRIORITY_PREF) ===
    Services.prefs.PREF_INVALID;
}

function testGetReturnsNullWhenPrefUnset(): Promise<void> {
  Services.prefs.clearUserPref(PREF);
  return getCommandPaletteSettings().then((result) => {
    assertEquals(result, null, "should return null when pref is not set");
    assert(isPrefUnset(), "pref should remain unset after get");
  });
}

function testGetReturnsEnabledTrue(): Promise<void> {
  Services.prefs.setBoolPref(PREF, true);
  try {
    return getCommandPaletteSettings().then((result) => {
      assert(result !== null, "result should not be null when pref is set");
      assertEquals(result!.enabled, true, "enabled should be true");
    });
  } finally {
    Services.prefs.clearUserPref(PREF);
  }
}

function testGetReturnsEnabledFalse(): Promise<void> {
  Services.prefs.setBoolPref(PREF, false);
  try {
    return getCommandPaletteSettings().then((result) => {
      assert(result !== null, "result should not be null when pref is set");
      assertEquals(result!.enabled, false, "enabled should be false");
    });
  } finally {
    Services.prefs.clearUserPref(PREF);
  }
}

function testGetReturnsNullOnRethrownPrefTypeMismatch(): Promise<void> {
  // Setting the pref as a STRING makes getPrefType !== PREF_BOOL, so the rpc
  // layer resolves to null and the dataManager forwards that null without
  // throwing — verifying the defensive guard.
  Services.prefs.setStringPref(PREF, "not-a-bool");
  try {
    return getCommandPaletteSettings().then((result) => {
      assertEquals(result, null, "should return null on pref type mismatch");
    });
  } finally {
    Services.prefs.clearUserPref(PREF);
  }
}

async function testSaveEmptyObjectIsNoOp(): Promise<void> {
  Services.prefs.setBoolPref(PREF, true);
  try {
    await saveCommandPaletteSettings({});
    assertEquals(
      readRawPref(),
      true,
      "empty object should not change the pref value",
    );
  } finally {
    Services.prefs.clearUserPref(PREF);
  }
}

async function testSaveSetsEnabledTrue(): Promise<void> {
  Services.prefs.setBoolPref(PREF, false);
  try {
    await saveCommandPaletteSettings({ enabled: true });
    assertEquals(readRawPref(), true, "save({enabled:true}) should set pref");
  } finally {
    Services.prefs.clearUserPref(PREF);
  }
}

async function testSaveSetsEnabledFalse(): Promise<void> {
  Services.prefs.setBoolPref(PREF, true);
  try {
    await saveCommandPaletteSettings({ enabled: false });
    assertEquals(readRawPref(), false, "save({enabled:false}) should set pref");
  } finally {
    Services.prefs.clearUserPref(PREF);
  }
}

async function testSaveCoercesToBoolean(): Promise<void> {
  // Boolean(undefined) === false; the dataManager wraps with Boolean() before
  // delegating to setBoolPref, so this must persist as a real boolean false.
  Services.prefs.setBoolPref(PREF, true);
  try {
    // deno-lint-ignore no-explicit-any
    await saveCommandPaletteSettings({ enabled: undefined } as any);
    assertEquals(
      readRawPref(),
      false,
      "save should coerce missing enabled to boolean false",
    );
  } finally {
    Services.prefs.clearUserPref(PREF);
  }
}

// ---------------------------------------------------------------------------
// categoryPriority (DEFAULT_CATEGORY_PRIORITY export + get/save round-trip)
// ---------------------------------------------------------------------------
//
// `parseCategoryPriority` lives privately inside the dataManager module; we
// exercise it end-to-end through the public `getCommandPaletteSettings` /
// `saveCommandPaletteSettings` surface by driving the underlying pref.
//
// The settings-side parser mirrors the chrome-side strict behavior: any
// non-string element rejects the WHOLE array and falls back to the default
// list. De-duplication is intentionally NOT performed — duplicates are
// nonsensical for priority order and the priority-index lookup returns the
// first match anyway.

/** Verifies the exported default has the documented shape (19 entries). */
function testDefaultCategoryPriorityExportShape(): void {
  assertEquals(
    DEFAULT_CATEGORY_PRIORITY.length,
    19,
    "DEFAULT_CATEGORY_PRIORITY should have 19 entries",
  );
  assertEquals(
    DEFAULT_CATEGORY_PRIORITY[0],
    "navigation",
    "first default priority should be 'navigation'",
  );
  assertEquals(
    DEFAULT_CATEGORY_PRIORITY[DEFAULT_CATEGORY_PRIORITY.length - 1],
    "bookmark-suggestions",
    "last default priority should be 'bookmark-suggestions'",
  );
}

/**
 * Verifies that an unset categoryPriority pref resolves to the default list.
 *
 * Mirrors the existing "no default in pref system" test pattern: clearing the
 * user pref makes `getPrefType` return PREF_INVALID, which causes the rpc
 * layer to resolve to `null`, which the dataManager forwards to its private
 * `parseCategoryPriority` — yielding a copy of `DEFAULT_CATEGORY_PRIORITY`.
 */
async function testGetReturnsDefaultCategoryPriorityWhenPrefUnset(): Promise<void> {
  Services.prefs.clearUserPref(CATEGORY_PRIORITY_PREF);
  assert(
    isCategoryPriorityPrefUnset(),
    "pref should be unset before the get call",
  );
  try {
    const result = await getCommandPaletteSettings();
    assert(result !== null, "result should not be null");
    assertStringArrayEqual(
      result!.categoryPriority,
      [...DEFAULT_CATEGORY_PRIORITY],
      "unset categoryPriority pref should yield DEFAULT_CATEGORY_PRIORITY",
    );
    assert(
      isCategoryPriorityPrefUnset(),
      "get should not mutate the pref",
    );
  } finally {
    Services.prefs.clearUserPref(CATEGORY_PRIORITY_PREF);
  }
}

/** Verifies a valid JSON string array pref is parsed into a string[]. */
async function testGetParsesValidCategoryPriority(): Promise<void> {
  Services.prefs.setStringPref(
    CATEGORY_PRIORITY_PREF,
    '["tabs","navigation","workspace"]',
  );
  try {
    const result = await getCommandPaletteSettings();
    assert(result !== null, "result should not be null");
    assertStringArrayEqual(
      result!.categoryPriority,
      ["tabs", "navigation", "workspace"],
      "valid JSON array should be parsed",
    );
  } finally {
    Services.prefs.clearUserPref(CATEGORY_PRIORITY_PREF);
  }
}

/**
 * Verifies that duplicated entries are returned as-is (NOT de-duplicated).
 *
 * De-duplication was removed to match the chrome-side strict parser; for
 * priority order duplicates are nonsensical and the priority-index lookup
 * returns the first match anyway.
 */
async function testGetPreservesDuplicateCategoryPriorityEntries(): Promise<void> {
  Services.prefs.setStringPref(
    CATEGORY_PRIORITY_PREF,
    '["tabs","tabs","navigation"]',
  );
  try {
    const result = await getCommandPaletteSettings();
    assert(result !== null, "result should not be null");
    assertStringArrayEqual(
      result!.categoryPriority,
      ["tabs", "tabs", "navigation"],
      "duplicates should be preserved as-is",
    );
  } finally {
    Services.prefs.clearUserPref(CATEGORY_PRIORITY_PREF);
  }
}

/** Verifies invalid JSON falls back to the default list. */
async function testGetFallsBackOnInvalidCategoryPriorityJson(): Promise<void> {
  Services.prefs.setStringPref(CATEGORY_PRIORITY_PREF, "not-json");
  try {
    const result = await getCommandPaletteSettings();
    assert(result !== null, "result should not be null");
    assertStringArrayEqual(
      result!.categoryPriority,
      [...DEFAULT_CATEGORY_PRIORITY],
      "invalid JSON should fall back to default",
    );
  } finally {
    Services.prefs.clearUserPref(CATEGORY_PRIORITY_PREF);
  }
}

/** Verifies a valid-JSON non-array falls back to the default list. */
async function testGetFallsBackOnNonArrayCategoryPriority(): Promise<void> {
  Services.prefs.setStringPref(CATEGORY_PRIORITY_PREF, '{"a":1}');
  try {
    const result = await getCommandPaletteSettings();
    assert(result !== null, "result should not be null");
    assertStringArrayEqual(
      result!.categoryPriority,
      [...DEFAULT_CATEGORY_PRIORITY],
      "JSON object should fall back to default",
    );
  } finally {
    Services.prefs.clearUserPref(CATEGORY_PRIORITY_PREF);
  }
}

/**
 * Verifies the strict behavior: an array with ANY non-string element rejects
 * the WHOLE array and falls back to the default list (matches the chrome-side
 * parser). A corrupted pref therefore surfaces identically in the settings
 * modal and the live palette.
 */
async function testGetFallsBackOnNonStringCategoryPriorityEntry(): Promise<void> {
  Services.prefs.setStringPref(
    CATEGORY_PRIORITY_PREF,
    '["a",123]',
  );
  try {
    const result = await getCommandPaletteSettings();
    assert(result !== null, "result should not be null");
    assertStringArrayEqual(
      result!.categoryPriority,
      [...DEFAULT_CATEGORY_PRIORITY],
      "array with a non-string element should fall back to default",
    );
  } finally {
    Services.prefs.clearUserPref(CATEGORY_PRIORITY_PREF);
  }
}

/** Verifies save writes a JSON.stringify'd array to the pref. */
async function testSaveCategoryPriorityStringifiesArray(): Promise<void> {
  Services.prefs.clearUserPref(CATEGORY_PRIORITY_PREF);
  try {
    await saveCommandPaletteSettings({
      categoryPriority: ["tabs", "navigation"],
    });
    assertEquals(
      readRawCategoryPriorityPref(),
      '["tabs","navigation"]',
      "save should JSON.stringify the array into the pref",
    );
  } finally {
    Services.prefs.clearUserPref(CATEGORY_PRIORITY_PREF);
  }
}

/** Verifies that omitting categoryPriority on save leaves the pref untouched. */
async function testSaveWithoutCategoryPriorityIsNoOp(): Promise<void> {
  Services.prefs.setStringPref(
    CATEGORY_PRIORITY_PREF,
    '["pre-existing"]',
  );
  try {
    await saveCommandPaletteSettings({ enabled: true });
    assertEquals(
      readRawCategoryPriorityPref(),
      '["pre-existing"]',
      "save without categoryPriority should not touch the pref",
    );
  } finally {
    Services.prefs.clearUserPref(CATEGORY_PRIORITY_PREF);
    Services.prefs.clearUserPref(PREF);
  }
}

const tests: TestCase[] = [
  { name: "getCommandPaletteSettings returns null when pref is unset", fn: testGetReturnsNullWhenPrefUnset },
  { name: "getCommandPaletteSettings returns { enabled: true }", fn: testGetReturnsEnabledTrue },
  { name: "getCommandPaletteSettings returns { enabled: false }", fn: testGetReturnsEnabledFalse },
  { name: "getCommandPaletteSettings returns null on pref type mismatch", fn: testGetReturnsNullOnRethrownPrefTypeMismatch },
  { name: "saveCommandPaletteSettings({}) is a no-op", fn: testSaveEmptyObjectIsNoOp },
  { name: "saveCommandPaletteSettings({ enabled: true }) sets pref", fn: testSaveSetsEnabledTrue },
  { name: "saveCommandPaletteSettings({ enabled: false }) sets pref", fn: testSaveSetsEnabledFalse },
  { name: "saveCommandPaletteSettings coerces enabled to boolean", fn: testSaveCoercesToBoolean },
  // categoryPriority
  { name: "DEFAULT_CATEGORY_PRIORITY is exported with 19 entries", fn: testDefaultCategoryPriorityExportShape },
  { name: "getCommandPaletteSettings yields DEFAULT_CATEGORY_PRIORITY when pref unset", fn: testGetReturnsDefaultCategoryPriorityWhenPrefUnset },
  { name: "getCommandPaletteSettings parses a valid JSON array", fn: testGetParsesValidCategoryPriority },
  { name: "getCommandPaletteSettings preserves duplicate categoryPriority entries (no de-dupe)", fn: testGetPreservesDuplicateCategoryPriorityEntries },
  { name: "getCommandPaletteSettings falls back on invalid JSON", fn: testGetFallsBackOnInvalidCategoryPriorityJson },
  { name: "getCommandPaletteSettings falls back on JSON non-array", fn: testGetFallsBackOnNonArrayCategoryPriority },
  { name: "getCommandPaletteSettings falls back on non-string entry (strict)", fn: testGetFallsBackOnNonStringCategoryPriorityEntry },
  { name: "saveCommandPaletteSettings JSON.stringify's the array into the pref", fn: testSaveCategoryPriorityStringifiesArray },
  { name: "saveCommandPaletteSettings without categoryPriority leaves the pref untouched", fn: testSaveWithoutCategoryPriorityIsNoOp },
];

export async function runAllTests(): Promise<void> {
  const originalEnabled = readRawPref();
  const originalCategoryPriority = readRawCategoryPriorityPref();
  try {
    await runTests("dataManager.test.ts (command-palette)", tests);
  } finally {
    // Restore original pref state so the test suite is hermetic.
    if (originalEnabled === null) {
      Services.prefs.clearUserPref(PREF);
    } else {
      Services.prefs.setBoolPref(PREF, originalEnabled);
    }
    if (originalCategoryPriority === null) {
      Services.prefs.clearUserPref(CATEGORY_PRIORITY_PREF);
    } else {
      Services.prefs.setStringPref(
        CATEGORY_PRIORITY_PREF,
        originalCategoryPriority,
      );
    }
  }
}
