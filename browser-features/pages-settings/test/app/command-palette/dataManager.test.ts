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
  isReservedShortcutPrefix,
  loadSelectableCommands,
  loadShortcuts,
  parseSelectableCommands,
  parseShortcuts,
  RESERVED_SHORTCUT_PREFIXES,
  saveCommandPaletteSettings,
  saveShortcuts,
} from "../../../src/app/command-palette/dataManager.ts";
import type {
  CommandPaletteFormData,
} from "../../../src/types/pref.ts";
import type {
  CommandPaletteShortcut,
  SelectableCommand,
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
 * NOTE: these prefs have no default values in the pref `.ini` files, but the
 * chrome-side command-palette config
 * (`browser-features/chrome/common/command-palette/config.ts`) registers pref
 * observers that re-seed each pref with its default value the moment
 * `clearUserPref` runs, so a pref can no longer be reliably observed as
 * PREF_INVALID in a live browser. The tests below therefore assert the
 * observable contract — default / seeded values are returned — rather than
 * relying on the pref being physically unset.
 */

const PREF = "floorp.commandPalette.enabled";
const WIDTH_PREF = "floorp.commandPalette.width";
const CATEGORY_PRIORITY_PREF = "floorp.commandPalette.categoryPriority";
const MAX_RESULTS_PREF = "floorp.commandPalette.maxResultsPerCategory";
const MAX_BOOKMARK_SUGGESTIONS_PREF = "floorp.commandPalette.maxBookmarkSuggestions";
const MAX_HISTORY_SUGGESTIONS_PREF = "floorp.commandPalette.maxHistorySuggestions";
const MAX_TABS_RESULTS_PREF = "floorp.commandPalette.maxTabsResults";
const SHORTCUTS_PREF = "floorp.commandPalette.shortcuts";
const SELECTABLE_COMMANDS_PREF = "floorp.commandPalette.selectableCommands";
const MAX_HEIGHT_PREF = "floorp.commandPalette.maxHeight";
const OFFSET_TOP_PREF = "floorp.commandPalette.offsetTop";
const HORIZONTAL_ALIGN_PREF = "floorp.commandPalette.horizontalAlign";
const FONT_SIZE_PREF = "floorp.commandPalette.fontSize";
const SHOW_TABS_PREF = "floorp.commandPalette.showTabs";
const SHOW_HISTORY_PREF = "floorp.commandPalette.showHistory";
const SHOW_BOOKMARKS_PREF = "floorp.commandPalette.showBookmarks";

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

/** Reads the raw int value of the max-results-per-category pref, or null when unset. */
function readRawMaxResultsPref(): number | null {
  if (
    Services.prefs.getPrefType(MAX_RESULTS_PREF) === Services.prefs.PREF_INVALID
  ) {
    return null;
  }
  try {
    return Services.prefs.getIntPref(MAX_RESULTS_PREF);
  } catch {
    return null;
  }
}

/** True when the max-results-per-category pref is currently unset (PREF_INVALID). */
function isMaxResultsPrefUnset(): boolean {
  return Services.prefs.getPrefType(MAX_RESULTS_PREF) ===
    Services.prefs.PREF_INVALID;
}

/** Reads the raw int value of the max-bookmark-suggestions pref, or null when unset. */
function readRawMaxBookmarkSuggestionsPref(): number | null {
  if (
    Services.prefs.getPrefType(MAX_BOOKMARK_SUGGESTIONS_PREF) ===
      Services.prefs.PREF_INVALID
  ) {
    return null;
  }
  try {
    return Services.prefs.getIntPref(MAX_BOOKMARK_SUGGESTIONS_PREF);
  } catch {
    return null;
  }
}

/** True when the max-bookmark-suggestions pref is currently unset (PREF_INVALID). */
function isMaxBookmarkSuggestionsPrefUnset(): boolean {
  return Services.prefs.getPrefType(MAX_BOOKMARK_SUGGESTIONS_PREF) ===
    Services.prefs.PREF_INVALID;
}

/** Reads the raw int value of the max-history-suggestions pref, or null when unset. */
function readRawMaxHistorySuggestionsPref(): number | null {
  if (
    Services.prefs.getPrefType(MAX_HISTORY_SUGGESTIONS_PREF) ===
      Services.prefs.PREF_INVALID
  ) {
    return null;
  }
  try {
    return Services.prefs.getIntPref(MAX_HISTORY_SUGGESTIONS_PREF);
  } catch {
    return null;
  }
}

/** True when the max-history-suggestions pref is currently unset (PREF_INVALID). */
function isMaxHistorySuggestionsPrefUnset(): boolean {
  return Services.prefs.getPrefType(MAX_HISTORY_SUGGESTIONS_PREF) ===
    Services.prefs.PREF_INVALID;
}

/** Reads the raw int value of the max-tabs-results pref, or null when unset. */
function readRawMaxTabsResultsPref(): number | null {
  if (
    Services.prefs.getPrefType(MAX_TABS_RESULTS_PREF) ===
      Services.prefs.PREF_INVALID
  ) {
    return null;
  }
  try {
    return Services.prefs.getIntPref(MAX_TABS_RESULTS_PREF);
  } catch {
    return null;
  }
}

/** True when the max-tabs-results pref is currently unset (PREF_INVALID). */
function isMaxTabsResultsPrefUnset(): boolean {
  return Services.prefs.getPrefType(MAX_TABS_RESULTS_PREF) ===
    Services.prefs.PREF_INVALID;
}

/** Reads the raw string value of the shortcuts pref, or null when unset. */
function readRawShortcutsPref(): string | null {
  if (
    Services.prefs.getPrefType(SHORTCUTS_PREF) !== Services.prefs.PREF_STRING
  ) {
    return null;
  }
  return Services.prefs.getStringPref(SHORTCUTS_PREF);
}

/** Reads the raw string value of the selectableCommands pref, or null when unset. */
function readRawSelectableCommandsPref(): string | null {
  if (
    Services.prefs.getPrefType(SELECTABLE_COMMANDS_PREF) !==
      Services.prefs.PREF_STRING
  ) {
    return null;
  }
  return Services.prefs.getStringPref(SELECTABLE_COMMANDS_PREF);
}

function testGetReturnsDefaultsWhenPrefUnset(): Promise<void> {
  // The chrome-side config observers re-seed this pref with its default
  // value the moment it is cleared, and `getCommandPaletteSettings` fills in
  // defaults for any pref that resolves to null — so the observable contract
  // is a full defaulted settings object, never null.
  // Clear every pref whose default this test asserts, so the result cannot
  // depend on state left behind by previously-run tests.
  Services.prefs.clearUserPref(PREF);
  Services.prefs.clearUserPref(WIDTH_PREF);
  Services.prefs.clearUserPref(MAX_HEIGHT_PREF);
  Services.prefs.clearUserPref(OFFSET_TOP_PREF);
  Services.prefs.clearUserPref(HORIZONTAL_ALIGN_PREF);
  Services.prefs.clearUserPref(FONT_SIZE_PREF);
  Services.prefs.clearUserPref(SHOW_TABS_PREF);
  Services.prefs.clearUserPref(SHOW_HISTORY_PREF);
  Services.prefs.clearUserPref(SHOW_BOOKMARKS_PREF);
  Services.prefs.clearUserPref(CATEGORY_PRIORITY_PREF);
  Services.prefs.clearUserPref(MAX_RESULTS_PREF);
  Services.prefs.clearUserPref(MAX_BOOKMARK_SUGGESTIONS_PREF);
  Services.prefs.clearUserPref(MAX_HISTORY_SUGGESTIONS_PREF);
  Services.prefs.clearUserPref(MAX_TABS_RESULTS_PREF);
  return getCommandPaletteSettings().then((result) => {
    assert(result !== null, "result should not be null when pref is unset");
    assertEquals(result!.enabled, true, "enabled should default to true");
    assertEquals(result!.width, 560, "width should default to 560");
    assertEquals(result!.maxHeight, 400, "maxHeight should default to 400");
    assertEquals(result!.offsetTop, 20, "offsetTop should default to 20");
    assertEquals(
      result!.horizontalAlign,
      "center",
      "horizontalAlign should default to 'center'",
    );
    assertEquals(result!.fontSize, 14, "fontSize should default to 14");
    assertEquals(result!.showTabs, true, "showTabs should default to true");
    assertEquals(
      result!.showHistory,
      true,
      "showHistory should default to true",
    );
    assertEquals(
      result!.showBookmarks,
      true,
      "showBookmarks should default to true",
    );
    assertEquals(
      result!.maxResultsPerCategory,
      5,
      "maxResultsPerCategory should default to 5",
    );
    assertEquals(
      result!.maxBookmarkSuggestions,
      5,
      "maxBookmarkSuggestions should default to 5",
    );
    assertEquals(
      result!.maxHistorySuggestions,
      5,
      "maxHistorySuggestions should default to 5",
    );
    assertEquals(
      result!.maxTabsResults,
      5,
      "maxTabsResults should default to 5",
    );
    assertStringArrayEqual(
      [...result!.categoryPriority],
      [...DEFAULT_CATEGORY_PRIORITY],
      "categoryPriority should default to DEFAULT_CATEGORY_PRIORITY",
    );
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

function testGetReturnsDefaultsOnEnabledPrefTypeMismatch(): Promise<void> {
  // Setting the pref as a STRING makes getPrefType !== PREF_BOOL, so the rpc
  // layer resolves to null and the dataManager defaults the field to true —
  // the full settings object is returned, never null.
  Services.prefs.setStringPref(PREF, "not-a-bool");
  try {
    return getCommandPaletteSettings().then((result) => {
      assert(
        result !== null,
        "result should not be null on pref type mismatch",
      );
      assertEquals(
        result!.enabled,
        true,
        "enabled should default to true on pref type mismatch",
      );
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
  // Patch-safe contract: `enabled` must only be written when present in the
  // patch, so saving another field (e.g. width) cannot disable the palette.
  // When a non-boolean `enabled` IS provided, the dataManager still coerces
  // it via Boolean() before delegating to setBoolPref.
  const widthPrefHadValue = Services.prefs.getPrefType(WIDTH_PREF) ===
    Services.prefs.PREF_INT;
  const originalWidth = widthPrefHadValue
    ? Services.prefs.getIntPref(WIDTH_PREF)
    : null;
  Services.prefs.setBoolPref(PREF, true);
  try {
    await saveCommandPaletteSettings({ width: 600 });
    assertEquals(
      readRawPref(),
      true,
      "save must not write enabled when it is absent from the patch",
    );

    await saveCommandPaletteSettings({ enabled: 1 as unknown as boolean });
    assertEquals(
      readRawPref(),
      true,
      "save should coerce a truthy non-boolean enabled to boolean true",
    );

    await saveCommandPaletteSettings({ enabled: 0 as unknown as boolean });
    assertEquals(
      readRawPref(),
      false,
      "save should coerce a falsy non-boolean enabled to boolean false",
    );
  } finally {
    Services.prefs.clearUserPref(PREF);
    if (originalWidth === null) {
      Services.prefs.clearUserPref(WIDTH_PREF);
    } else {
      Services.prefs.setIntPref(WIDTH_PREF, originalWidth);
    }
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

/** Verifies the exported default has the documented shape (18 entries). */
function testDefaultCategoryPriorityExportShape(): void {
  assertEquals(
    DEFAULT_CATEGORY_PRIORITY.length,
    18,
    "DEFAULT_CATEGORY_PRIORITY should have 18 entries",
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
 * Verifies that an unset (or default-seeded) categoryPriority pref resolves
 * to the default list.
 *
 * The chrome-side config observers re-seed this pref with the default JSON
 * the moment it is cleared, so the "pref should be unset" precondition no
 * longer holds in a live browser. The observable contract is unchanged:
 * whether the pref is unset (rpc resolves to null) or holds the seeded
 * default JSON, `getCommandPaletteSettings` yields a copy of
 * `DEFAULT_CATEGORY_PRIORITY`.
 */
async function testGetReturnsDefaultCategoryPriorityWhenPrefUnset(): Promise<void> {
  Services.prefs.clearUserPref(CATEGORY_PRIORITY_PREF);
  try {
    const result = await getCommandPaletteSettings();
    assert(result !== null, "result should not be null");
    assertStringArrayEqual(
      result!.categoryPriority,
      [...DEFAULT_CATEGORY_PRIORITY],
      "unset or default-seeded categoryPriority pref should yield DEFAULT_CATEGORY_PRIORITY",
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

// ---------------------------------------------------------------------------
// maxResultsPerCategory (default 5, clamped to [1, 20] via clampInt)
// ---------------------------------------------------------------------------
//
// `clampInt` rounds, then clamps into the bounds. Out-of-range values on read
// AND on save are clamped, so a corrupted pref cannot break the palette. The
// field is always present on the returned object (it has a numeric default),
// unlike `enabled` which defaults only when the pref is unset.

/** Verifies the default (5) is returned when the pref is unset. */
async function testGetReturnsDefaultMaxResultsWhenPrefUnset(): Promise<void> {
  Services.prefs.clearUserPref(MAX_RESULTS_PREF);
  assert(
    isMaxResultsPrefUnset(),
    "pref should be unset before the get call",
  );
  try {
    const result = await getCommandPaletteSettings();
    assert(result !== null, "result should not be null");
    assertEquals(
      result!.maxResultsPerCategory,
      5,
      "unset maxResultsPerCategory pref should yield the default 5",
    );
    assert(
      isMaxResultsPrefUnset(),
      "get should not mutate the pref",
    );
  } finally {
    Services.prefs.clearUserPref(MAX_RESULTS_PREF);
  }
}

/** Verifies a valid int value is read through unchanged. */
async function testGetReadsValidMaxResults(): Promise<void> {
  Services.prefs.setIntPref(MAX_RESULTS_PREF, 10);
  try {
    const result = await getCommandPaletteSettings();
    assert(result !== null, "result should not be null");
    assertEquals(
      result!.maxResultsPerCategory,
      10,
      "valid int 10 should be returned as-is",
    );
  } finally {
    Services.prefs.clearUserPref(MAX_RESULTS_PREF);
  }
}

/** Verifies a value below the min (1) is clamped up to 1. */
async function testGetClampsMaxResultsBelowMin(): Promise<void> {
  Services.prefs.setIntPref(MAX_RESULTS_PREF, 0);
  try {
    const result = await getCommandPaletteSettings();
    assert(result !== null, "result should not be null");
    assertEquals(
      result!.maxResultsPerCategory,
      1,
      "0 should be clamped up to the min (1)",
    );
  } finally {
    Services.prefs.clearUserPref(MAX_RESULTS_PREF);
  }
}

/** Verifies a negative value is clamped up to the min (1). */
async function testGetClampsMaxResultsNegative(): Promise<void> {
  Services.prefs.setIntPref(MAX_RESULTS_PREF, -5);
  try {
    const result = await getCommandPaletteSettings();
    assert(result !== null, "result should not be null");
    assertEquals(
      result!.maxResultsPerCategory,
      1,
      "-5 should be clamped up to the min (1)",
    );
  } finally {
    Services.prefs.clearUserPref(MAX_RESULTS_PREF);
  }
}

/** Verifies a value above the max (20) is clamped down to 20. */
async function testGetClampsMaxResultsAboveMax(): Promise<void> {
  Services.prefs.setIntPref(MAX_RESULTS_PREF, 100);
  try {
    const result = await getCommandPaletteSettings();
    assert(result !== null, "result should not be null");
    assertEquals(
      result!.maxResultsPerCategory,
      20,
      "100 should be clamped down to the max (20)",
    );
  } finally {
    Services.prefs.clearUserPref(MAX_RESULTS_PREF);
  }
}

/** Verifies save writes the in-range value through to the pref. */
async function testSaveMaxResultsWritesValue(): Promise<void> {
  Services.prefs.clearUserPref(MAX_RESULTS_PREF);
  try {
    await saveCommandPaletteSettings({ maxResultsPerCategory: 7 });
    assertEquals(
      readRawMaxResultsPref(),
      7,
      "save should persist 7 into the pref",
    );
  } finally {
    Services.prefs.clearUserPref(MAX_RESULTS_PREF);
  }
}

/** Verifies save clamps an above-max value before persisting. */
async function testSaveMaxResultsClampsAboveMax(): Promise<void> {
  Services.prefs.clearUserPref(MAX_RESULTS_PREF);
  try {
    await saveCommandPaletteSettings({ maxResultsPerCategory: 99 });
    assertEquals(
      readRawMaxResultsPref(),
      20,
      "save should clamp 99 down to the max (20) before persisting",
    );
  } finally {
    Services.prefs.clearUserPref(MAX_RESULTS_PREF);
  }
}

/** Verifies save clamps a below-min value before persisting. */
async function testSaveMaxResultsClampsBelowMin(): Promise<void> {
  Services.prefs.clearUserPref(MAX_RESULTS_PREF);
  try {
    await saveCommandPaletteSettings({ maxResultsPerCategory: 0 });
    assertEquals(
      readRawMaxResultsPref(),
      1,
      "save should clamp 0 up to the min (1) before persisting",
    );
  } finally {
    Services.prefs.clearUserPref(MAX_RESULTS_PREF);
  }
}

/** Verifies that omitting maxResultsPerCategory on save leaves the pref untouched. */
async function testSaveWithoutMaxResultsIsNoOp(): Promise<void> {
  Services.prefs.setIntPref(MAX_RESULTS_PREF, 13);
  try {
    await saveCommandPaletteSettings({ enabled: true });
    assertEquals(
      readRawMaxResultsPref(),
      13,
      "save without maxResultsPerCategory should not touch the pref",
    );
  } finally {
    Services.prefs.clearUserPref(MAX_RESULTS_PREF);
    Services.prefs.clearUserPref(PREF);
  }
}

// ---------------------------------------------------------------------------
// maxBookmarkSuggestions / maxHistorySuggestions / maxTabsResults
// (default 5, clamped to [1, 20] via clampInt — same shape as maxResultsPerCategory)
// ---------------------------------------------------------------------------
//
// The three dynamic-search limit prefs share identical semantics with
// `maxResultsPerCategory`: each defaults to 5 when unset, is clamped into
// [1, 20] on both read and save, and is always present on the returned
// object. We parameterize the tests via `testMaxPref` to avoid triplicating
// the assertions while keeping each pref's read/clamp/save path exercised
// end-to-end through the public dataManager surface.

/**
 * Builds the standard 5-test suite for one of the dynamic-search limit prefs:
 * default-when-unset, read-valid, clamp-above-max, clamp-below-min, and
 * save-writes-value. Mirrors the `maxResultsPerCategory` test block above.
 */
function testMaxPref(
  prefName: string,
  fieldName:
    | "maxBookmarkSuggestions"
    | "maxHistorySuggestions"
    | "maxTabsResults",
  readRaw: () => number | null,
  isUnset: () => boolean,
): TestCase[] {
  return [
    {
      name: `${fieldName}: default 5 when unset`,
      fn: async () => {
        Services.prefs.clearUserPref(prefName);
        assert(isUnset(), "pref should be unset before get");
        try {
          const result = await getCommandPaletteSettings();
          assert(result !== null, "result not null");
          assertEquals(result![fieldName], 5, `${fieldName} default`);
          assert(isUnset(), "get should not mutate the pref");
        } finally {
          Services.prefs.clearUserPref(prefName);
        }
      },
    },
    {
      name: `${fieldName}: reads valid value 10`,
      fn: async () => {
        Services.prefs.setIntPref(prefName, 10);
        try {
          const result = await getCommandPaletteSettings();
          assert(result !== null, "result not null");
          assertEquals(result![fieldName], 10, `${fieldName} reads 10`);
        } finally {
          Services.prefs.clearUserPref(prefName);
        }
      },
    },
    {
      name: `${fieldName}: clamps above max (100→20)`,
      fn: async () => {
        Services.prefs.setIntPref(prefName, 100);
        try {
          const result = await getCommandPaletteSettings();
          assert(result !== null, "result not null");
          assertEquals(result![fieldName], 20, `${fieldName} clamps 100→20`);
        } finally {
          Services.prefs.clearUserPref(prefName);
        }
      },
    },
    {
      name: `${fieldName}: clamps below min (0→1)`,
      fn: async () => {
        Services.prefs.setIntPref(prefName, 0);
        try {
          const result = await getCommandPaletteSettings();
          assert(result !== null, "result not null");
          assertEquals(result![fieldName], 1, `${fieldName} clamps 0→1`);
        } finally {
          Services.prefs.clearUserPref(prefName);
        }
      },
    },
    {
      name: `${fieldName}: save writes the value`,
      fn: async () => {
        Services.prefs.clearUserPref(prefName);
        try {
          const patch: Partial<CommandPaletteFormData> = {};
          patch[fieldName] = 7;
          await saveCommandPaletteSettings(patch);
          assertEquals(readRaw(), 7, `${fieldName} save writes 7`);
        } finally {
          Services.prefs.clearUserPref(prefName);
        }
      },
    },
  ];
}

// ---------------------------------------------------------------------------
// isReservedShortcutPrefix / RESERVED_SHORTCUT_PREFIXES
// ---------------------------------------------------------------------------
//
// Prefixes "s" (@s = built-in web search), "t" (@t = built-in open-tabs
// search), "b" (@b = built-in bookmark search) and "h" (@h = built-in history
// search) are reserved for built-in command palette behavior: the settings UI
// must reject user shortcuts that use them. Matching is case-sensitive, so
// uppercase variants are NOT reserved.
//
// KEEP IN SYNC with the chrome-side RESERVED_SHORTCUT_PREFIXES in
// browser-features/chrome/common/command-palette/config.ts — if one side
// changes, the other must change too.

/** "s" (@s web search) is reserved. */
function testReservedPrefixS(): void {
  assertEquals(
    isReservedShortcutPrefix("s"),
    true,
    "'s' (@s web search) should be reserved",
  );
}

/** "t" (@t open-tabs search) is reserved. */
function testReservedPrefixT(): void {
  assertEquals(
    isReservedShortcutPrefix("t"),
    true,
    "'t' (@t open-tabs search) should be reserved",
  );
}

/** "b" (@b bookmark search) is reserved. */
function testReservedPrefixB(): void {
  assertEquals(
    isReservedShortcutPrefix("b"),
    true,
    "'b' (@b bookmark search) should be reserved",
  );
}

/** "h" (@h history search) is reserved. */
function testReservedPrefixH(): void {
  assertEquals(
    isReservedShortcutPrefix("h"),
    true,
    "'h' (@h history search) should be reserved",
  );
}

/** Unreserved prefixes are rejected. */
function testReservedPrefixNonReserved(): void {
  assertEquals(
    isReservedShortcutPrefix("gh"),
    false,
    "unreserved prefix 'gh' should not be reserved",
  );
}

/** The empty string is not a reserved prefix. */
function testReservedPrefixEmpty(): void {
  assertEquals(
    isReservedShortcutPrefix(""),
    false,
    "empty prefix should not be reserved",
  );
}

/** Matching is case-sensitive — uppercase is NOT reserved. */
function testReservedPrefixCaseSensitive(): void {
  assertEquals(
    isReservedShortcutPrefix("S"),
    false,
    "uppercase 'S' should NOT be reserved (case-sensitive match)",
  );
}

/** The exported list has exactly the documented shape ["s", "t", "b", "h"]. */
function testReservedPrefixesExport(): void {
  assertEquals(
    RESERVED_SHORTCUT_PREFIXES.length,
    4,
    "RESERVED_SHORTCUT_PREFIXES should have exactly 4 entries",
  );
  assertStringArrayEqual(
    [...RESERVED_SHORTCUT_PREFIXES],
    ["s", "t", "b", "h"],
    "RESERVED_SHORTCUT_PREFIXES must equal ['s', 't', 'b', 'h']",
  );
}

// ---------------------------------------------------------------------------
// parseShortcuts — fallback to [] on malformed input
// ---------------------------------------------------------------------------

/** Valid JSON array of well-formed shortcut objects passes through. */
function testParseShortcutsValid(): void {
  const input = JSON.stringify([
    { prefix: "gh", commandId: "floorp-open-hub" },
    { prefix: "fb", commandId: "floorp-open-feedback" },
  ]);
  const result = parseShortcuts(input);
  assertEquals(result.length, 2, "valid array should return 2 entries");
  assertEquals(result[0].prefix, "gh", "first prefix");
  assertEquals(result[0].commandId, "floorp-open-hub", "first commandId");
  assertEquals(result[1].prefix, "fb", "second prefix");
  assertEquals(result[1].commandId, "floorp-open-feedback", "second commandId");
}

/** null input yields empty array. */
function testParseShortcutsNull(): void {
  const result = parseShortcuts(null);
  assertEquals(result.length, 0, "null should return []");
}

/** Empty string yields empty array. */
function testParseShortcutsEmpty(): void {
  const result = parseShortcuts("");
  assertEquals(result.length, 0, "empty string should return []");
}

/** Invalid JSON yields empty array. */
function testParseShortcutsInvalidJson(): void {
  const result = parseShortcuts("not-json{{{");
  assertEquals(result.length, 0, "invalid JSON should return []");
}

/** Valid JSON non-array yields empty array. */
function testParseShortcutsNonArray(): void {
  const result = parseShortcuts('{"a":1}');
  assertEquals(result.length, 0, "JSON object should return []");
}

/** Array with a non-object element yields empty array. */
function testParseShortcutsNonObjectElement(): void {
  const result = parseShortcuts('["just-a-string"]');
  assertEquals(result.length, 0, "non-object element should return []");
}

/** Array with numeric prefix yields empty array. */
function testParseShortcutsNumericPrefix(): void {
  const result = parseShortcuts('[{"prefix":123,"commandId":"x"}]');
  assertEquals(result.length, 0, "numeric prefix should return []");
}

/** Array with missing commandId yields empty array. */
function testParseShortcutsMissingCommandId(): void {
  const result = parseShortcuts('[{"prefix":"gh"}]');
  assertEquals(result.length, 0, "missing commandId should return []");
}

/** Duplicate prefixes are deduped: only the first occurrence of each survives. */
function testParseShortcutsDedupesByPrefix(): void {
  const result = parseShortcuts(
    JSON.stringify([
      { prefix: "gh", commandId: "first" },
      { prefix: "gh", commandId: "second" },
      { prefix: "fb", commandId: "feedback" },
      { prefix: "fb", commandId: "other" },
      { prefix: "zz", commandId: "unique" },
    ]),
  );
  assertEquals(result.length, 3, "duplicate prefixes should be deduped");
  assertEquals(result[0].prefix, "gh", "first 'gh' entry is retained");
  assertEquals(result[0].commandId, "first", "first occurrence wins");
  assertEquals(result[1].prefix, "fb", "first 'fb' entry is retained");
  assertEquals(result[1].commandId, "feedback", "first occurrence wins");
  assertEquals(result[2].prefix, "zz", "unique entry is kept");
}

// ---------------------------------------------------------------------------
// parseSelectableCommands — fallback to [] on malformed input
// ---------------------------------------------------------------------------

/** Valid JSON array of well-formed selectable-command objects passes through. */
function testParseSelectableCommandsValid(): void {
  const input = JSON.stringify([
    { id: "cmd-1", label: "Open Hub", category: "navigation" },
    { id: "cmd-2", label: "Close Tab", category: "tabs" },
  ]);
  const result = parseSelectableCommands(input);
  assertEquals(result.length, 2, "valid array should return 2 entries");
  assertEquals(result[0].id, "cmd-1", "first id");
  assertEquals(result[0].label, "Open Hub", "first label");
  assertEquals(result[0].category, "navigation", "first category");
  assertEquals(result[1].id, "cmd-2", "second id");
}

/** null input yields empty array. */
function testParseSelectableCommandsNull(): void {
  assertEquals(parseSelectableCommands(null).length, 0, "null → []");
}

/** Empty string yields empty array. */
function testParseSelectableCommandsEmpty(): void {
  assertEquals(parseSelectableCommands("").length, 0, "empty → []");
}

/** Invalid JSON yields empty array. */
function testParseSelectableCommandsInvalidJson(): void {
  assertEquals(parseSelectableCommands("bad").length, 0, "bad JSON → []");
}

/** Valid JSON non-array yields empty array. */
function testParseSelectableCommandsNonArray(): void {
  assertEquals(parseSelectableCommands("{}").length, 0, "object → []");
}

/** Array with missing category yields empty array. */
function testParseSelectableCommandsMissingCategory(): void {
  assertEquals(
    parseSelectableCommands('[{"id":"x","label":"y"}]').length,
    0,
    "missing category → []",
  );
}

// ---------------------------------------------------------------------------
// loadShortcuts / saveShortcuts round-trip
// ---------------------------------------------------------------------------

/**
 * loadShortcuts returns [] when no user shortcuts are configured.
 *
 * The chrome-side config observers re-seed the shortcuts pref with "[]" the
 * moment it is cleared, so the "pref should be unset" precondition no longer
 * holds in a live browser. The observable contract is unchanged: with no user
 * shortcuts configured, loadShortcuts() yields [].
 */
async function testLoadShortcutsEmpty(): Promise<void> {
  Services.prefs.clearUserPref(SHORTCUTS_PREF);
  try {
    const result = await loadShortcuts();
    assertEquals(result.length, 0, "no configured shortcuts should yield []");
  } finally {
    Services.prefs.clearUserPref(SHORTCUTS_PREF);
  }
}

/** saveShortcuts → loadShortcuts round-trip. */
async function testSaveAndLoadShortcutsRoundTrip(): Promise<void> {
  const shortcuts: CommandPaletteShortcut[] = [
    { prefix: "gh", commandId: "floorp-open-hub" },
  ];
  try {
    await saveShortcuts(shortcuts);
    assertEquals(
      readRawShortcutsPref(),
      JSON.stringify(shortcuts),
      "pref should contain JSON string",
    );
    const loaded = await loadShortcuts();
    assertEquals(loaded.length, 1, "loaded should have 1 entry");
    assertEquals(loaded[0].prefix, "gh", "loaded prefix");
    assertEquals(loaded[0].commandId, "floorp-open-hub", "loaded commandId");
  } finally {
    Services.prefs.clearUserPref(SHORTCUTS_PREF);
  }
}

// ---------------------------------------------------------------------------
// loadSelectableCommands
// ---------------------------------------------------------------------------

/**
 * loadSelectableCommands returns [] when no command catalog is cached.
 *
 * The chrome-side config observers re-seed this pref (with "[]") the moment
 * it is cleared, so the "pref should be unset" precondition no longer holds
 * in a live browser. The observable contract is unchanged: with no cached
 * catalog, loadSelectableCommands() yields [].
 */
async function testLoadSelectableCommandsEmpty(): Promise<void> {
  Services.prefs.clearUserPref(SELECTABLE_COMMANDS_PREF);
  try {
    const result = await loadSelectableCommands();
    assertEquals(result.length, 0, "no cached catalog should yield []");
  } finally {
    Services.prefs.clearUserPref(SELECTABLE_COMMANDS_PREF);
  }
}

/** loadSelectableCommands parses a valid pref. */
async function testLoadSelectableCommandsParsesValid(): Promise<void> {
  const commands: SelectableCommand[] = [
    { id: "cmd-1", label: "Open Hub", category: "navigation" },
  ];
  Services.prefs.setStringPref(
    SELECTABLE_COMMANDS_PREF,
    JSON.stringify(commands),
  );
  try {
    const result = await loadSelectableCommands();
    assertEquals(result.length, 1, "should have 1 entry");
    assertEquals(result[0].id, "cmd-1", "id");
    assertEquals(result[0].label, "Open Hub", "label");
    assertEquals(result[0].category, "navigation", "category");
  } finally {
    Services.prefs.clearUserPref(SELECTABLE_COMMANDS_PREF);
  }
}

const tests: TestCase[] = [
  { name: "getCommandPaletteSettings returns full defaults when pref is unset", fn: testGetReturnsDefaultsWhenPrefUnset },
  { name: "getCommandPaletteSettings returns { enabled: true }", fn: testGetReturnsEnabledTrue },
  { name: "getCommandPaletteSettings returns { enabled: false }", fn: testGetReturnsEnabledFalse },
  { name: "getCommandPaletteSettings returns full defaults on pref type mismatch", fn: testGetReturnsDefaultsOnEnabledPrefTypeMismatch },
  { name: "saveCommandPaletteSettings({}) is a no-op", fn: testSaveEmptyObjectIsNoOp },
  { name: "saveCommandPaletteSettings({ enabled: true }) sets pref", fn: testSaveSetsEnabledTrue },
  { name: "saveCommandPaletteSettings({ enabled: false }) sets pref", fn: testSaveSetsEnabledFalse },
  { name: "saveCommandPaletteSettings coerces enabled to boolean", fn: testSaveCoercesToBoolean },
  // categoryPriority
  { name: "DEFAULT_CATEGORY_PRIORITY is exported with 18 entries", fn: testDefaultCategoryPriorityExportShape },
  { name: "getCommandPaletteSettings yields DEFAULT_CATEGORY_PRIORITY when pref unset or default-seeded", fn: testGetReturnsDefaultCategoryPriorityWhenPrefUnset },
  { name: "getCommandPaletteSettings parses a valid JSON array", fn: testGetParsesValidCategoryPriority },
  { name: "getCommandPaletteSettings preserves duplicate categoryPriority entries (no de-dupe)", fn: testGetPreservesDuplicateCategoryPriorityEntries },
  { name: "getCommandPaletteSettings falls back on invalid JSON", fn: testGetFallsBackOnInvalidCategoryPriorityJson },
  { name: "getCommandPaletteSettings falls back on JSON non-array", fn: testGetFallsBackOnNonArrayCategoryPriority },
  { name: "getCommandPaletteSettings falls back on non-string entry (strict)", fn: testGetFallsBackOnNonStringCategoryPriorityEntry },
  { name: "saveCommandPaletteSettings JSON.stringify's the array into the pref", fn: testSaveCategoryPriorityStringifiesArray },
  { name: "saveCommandPaletteSettings without categoryPriority leaves the pref untouched", fn: testSaveWithoutCategoryPriorityIsNoOp },
  // maxResultsPerCategory
  { name: "getCommandPaletteSettings yields 5 for maxResultsPerCategory when pref unset", fn: testGetReturnsDefaultMaxResultsWhenPrefUnset },
  { name: "getCommandPaletteSettings reads a valid maxResultsPerCategory int", fn: testGetReadsValidMaxResults },
  { name: "getCommandPaletteSettings clamps maxResultsPerCategory 0 up to 1", fn: testGetClampsMaxResultsBelowMin },
  { name: "getCommandPaletteSettings clamps negative maxResultsPerCategory up to 1", fn: testGetClampsMaxResultsNegative },
  { name: "getCommandPaletteSettings clamps maxResultsPerCategory 100 down to 20", fn: testGetClampsMaxResultsAboveMax },
  { name: "saveCommandPaletteSettings writes maxResultsPerCategory into the pref", fn: testSaveMaxResultsWritesValue },
  { name: "saveCommandPaletteSettings clamps maxResultsPerCategory 99 down to 20", fn: testSaveMaxResultsClampsAboveMax },
  { name: "saveCommandPaletteSettings clamps maxResultsPerCategory 0 up to 1", fn: testSaveMaxResultsClampsBelowMin },
  { name: "saveCommandPaletteSettings without maxResultsPerCategory leaves the pref untouched", fn: testSaveWithoutMaxResultsIsNoOp },
  // maxBookmarkSuggestions / maxHistorySuggestions / maxTabsResults
  // (5 parameterized tests each: default, read-valid, clamp-above, clamp-below, save)
  ...testMaxPref(
    MAX_BOOKMARK_SUGGESTIONS_PREF,
    "maxBookmarkSuggestions",
    readRawMaxBookmarkSuggestionsPref,
    isMaxBookmarkSuggestionsPrefUnset,
  ),
  ...testMaxPref(
    MAX_HISTORY_SUGGESTIONS_PREF,
    "maxHistorySuggestions",
    readRawMaxHistorySuggestionsPref,
    isMaxHistorySuggestionsPrefUnset,
  ),
  ...testMaxPref(
    MAX_TABS_RESULTS_PREF,
    "maxTabsResults",
    readRawMaxTabsResultsPref,
    isMaxTabsResultsPrefUnset,
  ),
  // isReservedShortcutPrefix / RESERVED_SHORTCUT_PREFIXES
  { name: "isReservedShortcutPrefix('s') is true (@s web search)", fn: testReservedPrefixS },
  { name: "isReservedShortcutPrefix('t') is true (@t open-tabs search)", fn: testReservedPrefixT },
  { name: "isReservedShortcutPrefix('b') is true (@b bookmark search)", fn: testReservedPrefixB },
  { name: "isReservedShortcutPrefix('h') is true (@h history search)", fn: testReservedPrefixH },
  { name: "isReservedShortcutPrefix('gh') is false", fn: testReservedPrefixNonReserved },
  { name: "isReservedShortcutPrefix('') is false", fn: testReservedPrefixEmpty },
  { name: "isReservedShortcutPrefix('S') is false (case-sensitive)", fn: testReservedPrefixCaseSensitive },
  { name: "RESERVED_SHORTCUT_PREFIXES equals ['s', 't', 'b', 'h']", fn: testReservedPrefixesExport },
  // parseShortcuts
  { name: "parseShortcuts: valid array passes through", fn: testParseShortcutsValid },
  { name: "parseShortcuts: null → []", fn: testParseShortcutsNull },
  { name: "parseShortcuts: empty string → []", fn: testParseShortcutsEmpty },
  { name: "parseShortcuts: invalid JSON → []", fn: testParseShortcutsInvalidJson },
  { name: "parseShortcuts: JSON object → []", fn: testParseShortcutsNonArray },
  { name: "parseShortcuts: non-object element → []", fn: testParseShortcutsNonObjectElement },
  { name: "parseShortcuts: numeric prefix → []", fn: testParseShortcutsNumericPrefix },
  { name: "parseShortcuts: missing commandId → []", fn: testParseShortcutsMissingCommandId },
  { name: "parseShortcuts: duplicate prefixes deduped (first wins)", fn: testParseShortcutsDedupesByPrefix },
  // parseSelectableCommands
  { name: "parseSelectableCommands: valid array passes through", fn: testParseSelectableCommandsValid },
  { name: "parseSelectableCommands: null → []", fn: testParseSelectableCommandsNull },
  { name: "parseSelectableCommands: empty string → []", fn: testParseSelectableCommandsEmpty },
  { name: "parseSelectableCommands: invalid JSON → []", fn: testParseSelectableCommandsInvalidJson },
  { name: "parseSelectableCommands: JSON object → []", fn: testParseSelectableCommandsNonArray },
  { name: "parseSelectableCommands: missing category → []", fn: testParseSelectableCommandsMissingCategory },
  // loadShortcuts / saveShortcuts
  { name: "loadShortcuts returns [] when no shortcuts configured", fn: testLoadShortcutsEmpty },
  { name: "saveShortcuts → loadShortcuts round-trip", fn: testSaveAndLoadShortcutsRoundTrip },
  // loadSelectableCommands
  { name: "loadSelectableCommands returns [] when no catalog cached", fn: testLoadSelectableCommandsEmpty },
  { name: "loadSelectableCommands parses a valid pref", fn: testLoadSelectableCommandsParsesValid },
];

export async function runAllTests(): Promise<void> {
  const originalEnabled = readRawPref();
  const originalCategoryPriority = readRawCategoryPriorityPref();
  const originalMaxResults = readRawMaxResultsPref();
  const originalMaxBookmarkSuggestions = readRawMaxBookmarkSuggestionsPref();
  const originalMaxHistorySuggestions = readRawMaxHistorySuggestionsPref();
  const originalMaxTabsResults = readRawMaxTabsResultsPref();
  const originalShortcuts = readRawShortcutsPref();
  const originalSelectableCommands = readRawSelectableCommandsPref();
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
    if (originalMaxResults === null) {
      Services.prefs.clearUserPref(MAX_RESULTS_PREF);
    } else {
      Services.prefs.setIntPref(MAX_RESULTS_PREF, originalMaxResults);
    }
    if (originalMaxBookmarkSuggestions === null) {
      Services.prefs.clearUserPref(MAX_BOOKMARK_SUGGESTIONS_PREF);
    } else {
      Services.prefs.setIntPref(
        MAX_BOOKMARK_SUGGESTIONS_PREF,
        originalMaxBookmarkSuggestions,
      );
    }
    if (originalMaxHistorySuggestions === null) {
      Services.prefs.clearUserPref(MAX_HISTORY_SUGGESTIONS_PREF);
    } else {
      Services.prefs.setIntPref(
        MAX_HISTORY_SUGGESTIONS_PREF,
        originalMaxHistorySuggestions,
      );
    }
    if (originalMaxTabsResults === null) {
      Services.prefs.clearUserPref(MAX_TABS_RESULTS_PREF);
    } else {
      Services.prefs.setIntPref(MAX_TABS_RESULTS_PREF, originalMaxTabsResults);
    }
    if (originalShortcuts === null) {
      Services.prefs.clearUserPref(SHORTCUTS_PREF);
    } else {
      Services.prefs.setStringPref(SHORTCUTS_PREF, originalShortcuts);
    }
    if (originalSelectableCommands === null) {
      Services.prefs.clearUserPref(SELECTABLE_COMMANDS_PREF);
    } else {
      Services.prefs.setStringPref(
        SELECTABLE_COMMANDS_PREF,
        originalSelectableCommands,
      );
    }
  }
}
