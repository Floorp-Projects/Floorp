// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import {
  clampInt,
  normalizeHorizontalAlign,
  parseSelectableCommands,
  parseShortcuts,
  WIDTH_BOUNDS,
  MAX_HEIGHT_BOUNDS,
  OFFSET_TOP_BOUNDS,
  FONT_SIZE_BOUNDS,
} from "../config.ts";
import type {
  CommandPaletteShortcut,
} from "../types.ts";

// ---------------------------------------------------------------------------
// clampInt
// ---------------------------------------------------------------------------

/** Verifies that in-range values are returned unchanged. */
function testClampIntReturnsInRangeValue(): void {
  assertEquals(
    clampInt(560, 400, 1000, 560),
    560,
    "in-range value should be returned as-is",
  );
}

/** Verifies that a value below the lower bound is clamped up to min. */
function testClampIntClampsBelowMin(): void {
  assertEquals(
    clampInt(100, 400, 1000, 560),
    400,
    "value below min should be clamped to min",
  );
}

/** Verifies that a value above the upper bound is clamped down to max. */
function testClampIntClampsAboveMax(): void {
  assertEquals(
    clampInt(5000, 400, 1000, 560),
    1000,
    "value above max should be clamped to max",
  );
}

/** Verifies that the exact lower bound is preserved. */
function testClampIntKeepsLowerBoundary(): void {
  assertEquals(
    clampInt(400, 400, 1000, 560),
    400,
    "value equal to min should stay at min",
  );
}

/** Verifies that the exact upper bound is preserved. */
function testClampIntKeepsUpperBoundary(): void {
  assertEquals(
    clampInt(1000, 400, 1000, 560),
    1000,
    "value equal to max should stay at max",
  );
}

/** Verifies that fractional values are rounded (Math.round) before clamping. */
function testClampIntRoundsFractionalUp(): void {
  assertEquals(
    clampInt(560.7, 400, 1000, 560),
    561,
    "560.7 should round to 561",
  );
}

/** Verifies that fractional values round half-up (banker's rounding not used). */
function testClampIntRoundsFractionalDown(): void {
  assertEquals(
    clampInt(560.3, 400, 1000, 560),
    560,
    "560.3 should round to 560",
  );
}

/** Verifies that .5 rounds toward +infinity (Math.round semantics). */
function testClampIntRoundsHalfUp(): void {
  // Math.round(2.5) === 3, Math.round(-2.5) === -2 (ties go toward +Infinity).
  assertEquals(
    clampInt(2.5, 0, 10, 5),
    3,
    "2.5 should round to 3 (ties toward +Infinity)",
  );
}

/** Verifies that NaN falls back to the fallback value. */
function testClampIntReturnsFallbackForNaN(): void {
  assertEquals(
    clampInt(NaN, 400, 1000, 560),
    560,
    "NaN should fall back to fallback",
  );
}

/** Verifies that +Infinity falls back to the fallback value. */
function testClampIntReturnsFallbackForPositiveInfinity(): void {
  assertEquals(
    clampInt(Infinity, 400, 1000, 560),
    560,
    "+Infinity should fall back to fallback",
  );
}

/** Verifies that -Infinity falls back to the fallback value. */
function testClampIntReturnsFallbackForNegativeInfinity(): void {
  assertEquals(
    clampInt(-Infinity, 400, 1000, 560),
    560,
    "-Infinity should fall back to fallback",
  );
}

/** Verifies that a negative in-range value within bounds is preserved. */
function testClampIntPreservesNegativeInRange(): void {
  assertEquals(
    clampInt(-5, -10, 10, 0),
    -5,
    "negative value within bounds should be preserved",
  );
}

/** Verifies that real pref bounds produce the documented defaults. */
function testClampIntWorksWithRealWidthBounds(): void {
  assertEquals(
    clampInt(560, WIDTH_BOUNDS.min, WIDTH_BOUNDS.max, 560),
    560,
    "default width 560 should remain 560 under WIDTH_BOUNDS",
  );
}

/** Verifies that real max-height bounds clamp an oversize value. */
function testClampIntWorksWithRealMaxHeightBounds(): void {
  assertEquals(
    clampInt(9999, MAX_HEIGHT_BOUNDS.min, MAX_HEIGHT_BOUNDS.max, 400),
    MAX_HEIGHT_BOUNDS.max,
    "oversize maxHeight should clamp to MAX_HEIGHT_BOUNDS.max",
  );
}

/** Verifies that real offset-top bounds clamp a negative value to 0. */
function testClampIntWorksWithRealOffsetTopBounds(): void {
  assertEquals(
    clampInt(-5, OFFSET_TOP_BOUNDS.min, OFFSET_TOP_BOUNDS.max, 20),
    OFFSET_TOP_BOUNDS.min,
    "negative offsetTop should clamp to OFFSET_TOP_BOUNDS.min (0)",
  );
}

/** Verifies that an in-range font-size value is preserved under FONT_SIZE_BOUNDS. */
function testClampIntWorksWithRealFontSizeBoundsInRange(): void {
  assertEquals(
    clampInt(14, FONT_SIZE_BOUNDS.min, FONT_SIZE_BOUNDS.max, 14),
    14,
    "font-size 14 should remain 14 under FONT_SIZE_BOUNDS",
  );
}

/** Verifies that a sub-min font-size value clamps up to FONT_SIZE_BOUNDS.min. */
function testClampIntClampsBelowMinViaFontSizeBounds(): void {
  assertEquals(
    clampInt(5, FONT_SIZE_BOUNDS.min, FONT_SIZE_BOUNDS.max, 14),
    FONT_SIZE_BOUNDS.min,
    "font-size 5 should clamp to FONT_SIZE_BOUNDS.min (11)",
  );
}

/** Verifies that a super-max font-size value clamps down to FONT_SIZE_BOUNDS.max. */
function testClampIntClampsAboveMaxViaFontSizeBounds(): void {
  assertEquals(
    clampInt(40, FONT_SIZE_BOUNDS.min, FONT_SIZE_BOUNDS.max, 14),
    FONT_SIZE_BOUNDS.max,
    "font-size 40 should clamp to FONT_SIZE_BOUNDS.max (22)",
  );
}

/** Verifies that the exact FONT_SIZE_BOUNDS lower boundary is preserved. */
function testClampIntKeepsLowerBoundaryViaFontSizeBounds(): void {
  assertEquals(
    clampInt(11, FONT_SIZE_BOUNDS.min, FONT_SIZE_BOUNDS.max, 14),
    FONT_SIZE_BOUNDS.min,
    "font-size 11 (== min) should stay at FONT_SIZE_BOUNDS.min",
  );
}

/** Verifies that the exact FONT_SIZE_BOUNDS upper boundary is preserved. */
function testClampIntKeepsUpperBoundaryViaFontSizeBounds(): void {
  assertEquals(
    clampInt(22, FONT_SIZE_BOUNDS.min, FONT_SIZE_BOUNDS.max, 14),
    FONT_SIZE_BOUNDS.max,
    "font-size 22 (== max) should stay at FONT_SIZE_BOUNDS.max",
  );
}

// ---------------------------------------------------------------------------
// normalizeHorizontalAlign
// ---------------------------------------------------------------------------

/** Verifies that "center" is preserved. */
function testNormalizeHorizontalAlignCenter(): void {
  assertEquals(
    normalizeHorizontalAlign("center"),
    "center",
    "'center' should normalize to 'center'",
  );
}

/** Verifies that "left" is preserved. */
function testNormalizeHorizontalAlignLeft(): void {
  assertEquals(
    normalizeHorizontalAlign("left"),
    "left",
    "'left' should normalize to 'left'",
  );
}

/** Verifies that "right" is preserved. */
function testNormalizeHorizontalAlignRight(): void {
  assertEquals(
    normalizeHorizontalAlign("right"),
    "right",
    "'right' should normalize to 'right'",
  );
}

/** Verifies that an arbitrary invalid string falls back to "center". */
function testNormalizeHorizontalAlignInvalidString(): void {
  assertEquals(
    normalizeHorizontalAlign("invalid"),
    "center",
    "invalid string should fall back to 'center'",
  );
}

/** Verifies that the empty string falls back to "center". */
function testNormalizeHorizontalAlignEmptyString(): void {
  assertEquals(
    normalizeHorizontalAlign(""),
    "center",
    "empty string should fall back to 'center'",
  );
}

/** Verifies that uppercase "CENTER" is NOT accepted (strict equality). */
function testNormalizeHorizontalAlignUppercaseRejected(): void {
  assertEquals(
    normalizeHorizontalAlign("CENTER"),
    "center",
    "'CENTER' should fall back to 'center' (strict match)",
  );
}

/** Verifies that an unrelated orientation like "top" falls back to "center". */
function testNormalizeHorizontalAlignTopFallsBack(): void {
  assertEquals(
    normalizeHorizontalAlign("top"),
    "center",
    "'top' should fall back to 'center'",
  );
}

// ---------------------------------------------------------------------------
// parseShortcuts(jsonStr, defaultVal)
// ---------------------------------------------------------------------------
//
// `parseShortcuts` is the chrome-side strict parser for the
// `floorp.commandPalette.shortcuts` pref. It accepts a JSON string and a
// fallback, returning the parsed array only when every element matches the
// `{prefix:string, commandId:string}` shape. Any malformed input rejects the
// WHOLE array and yields `defaultVal` (the settings parser mirrors this but
// always returns []). We pass a sentinel default to distinguish "rejected"
// from "legitimately empty".

const SHORTCUTS_DEFAULT: CommandPaletteShortcut[] = [
  { prefix: "sentinel", commandId: "sentinel-cmd" },
];

/** Verifies a valid two-element array round-trips through the parser. */
function testParseShortcutsValidArray(): void {
  const input = JSON.stringify([
    { prefix: "gh", commandId: "floorp-open-hub" },
    { prefix: "gs", commandId: "floorp-open-settings" },
  ]);
  const result = parseShortcuts(input, SHORTCUTS_DEFAULT);
  assertEquals(result.length, 2, "valid array should yield 2 entries");
  assertEquals(result[0].prefix, "gh", "first prefix preserved");
  assertEquals(
    result[0].commandId,
    "floorp-open-hub",
    "first commandId preserved",
  );
  assertEquals(result[1].prefix, "gs", "second prefix preserved");
}

/** Verifies a single-element valid array is returned. */
function testParseShortcutsSingleElement(): void {
  const input = JSON.stringify([{ prefix: "x", commandId: "y" }]);
  const result = parseShortcuts(input, SHORTCUTS_DEFAULT);
  assertEquals(result.length, 1, "single valid element should yield 1");
  assertEquals(result[0].prefix, "x", "prefix preserved");
}

/** Verifies an empty JSON array ("[]") is accepted as a valid (empty) array. */
function testParseShortcutsEmptyArray(): void {
  const result = parseShortcuts("[]", SHORTCUTS_DEFAULT);
  assertEquals(result.length, 0, "empty JSON array should yield 0 entries");
}

/** Verifies an empty input string falls back to defaultVal (not parsed). */
function testParseShortcutsEmptyStringFallsBack(): void {
  const result = parseShortcuts("", SHORTCUTS_DEFAULT);
  assertEquals(
    result.length,
    SHORTCUTS_DEFAULT.length,
    "empty string should yield defaultVal",
  );
  assertEquals(result[0].prefix, "sentinel", "defaultVal returned verbatim");
}

/** Verifies a literal "null" JSON value falls back to defaultVal. */
function testParseShortcutsJsonNullFallsBack(): void {
  const result = parseShortcuts("null", SHORTCUTS_DEFAULT);
  assertEquals(
    result.length,
    SHORTCUTS_DEFAULT.length,
    "'null' is not an array -> defaultVal",
  );
}

/** Verifies invalid JSON falls back to defaultVal. */
function testParseShortcutsInvalidJsonFallsBack(): void {
  const result = parseShortcuts("not-json{", SHORTCUTS_DEFAULT);
  assertEquals(
    result.length,
    SHORTCUTS_DEFAULT.length,
    "invalid JSON should yield defaultVal",
  );
}

/** Verifies valid JSON that is NOT an array falls back to defaultVal. */
function testParseShortcutsNonArrayFallsBack(): void {
  const result = parseShortcuts('{"prefix":"gh","commandId":"x"}', SHORTCUTS_DEFAULT);
  assertEquals(
    result.length,
    SHORTCUTS_DEFAULT.length,
    "JSON object (non-array) should yield defaultVal",
  );
}

/**
 * Verifies the strict shape guard: a single element whose `prefix` is a
 * number (not string) rejects the WHOLE array, returning defaultVal.
 */
function testParseShortcutsNumericPrefixRejectsAll(): void {
  const input = JSON.stringify([
    { prefix: "gh", commandId: "ok" },
    { prefix: 123, commandId: "bad" },
  ]);
  const result = parseShortcuts(input, SHORTCUTS_DEFAULT);
  assertEquals(
    result.length,
    SHORTCUTS_DEFAULT.length,
    "any non-string prefix should reject the whole array -> defaultVal",
  );
}

/**
 * Verifies the strict shape guard: a missing `commandId` (present as
 * undefined, dropped by JSON.stringify) rejects the whole array.
 */
function testParseShortcutsMissingCommandIdRejectsAll(): void {
  // JSON.stringify drops undefined keys, so this serializes to {"prefix":"gh"}.
  const input = JSON.stringify([{ prefix: "gh" }]);
  const result = parseShortcuts(input, SHORTCUTS_DEFAULT);
  assertEquals(
    result.length,
    SHORTCUTS_DEFAULT.length,
    "missing commandId should reject the whole array -> defaultVal",
  );
}

/** Verifies a non-string `commandId` (number) rejects the whole array. */
function testParseShortcutsNumericCommandIdRejectsAll(): void {
  const input = JSON.stringify([
    { prefix: "gh", commandId: 99 },
  ]);
  const result = parseShortcuts(input, SHORTCUTS_DEFAULT);
  assertEquals(
    result.length,
    SHORTCUTS_DEFAULT.length,
    "numeric commandId should reject the whole array -> defaultVal",
  );
}

/**
 * Verifies duplicate prefixes are deduped: only the first occurrence of each
 * prefix survives.
 */
function testParseShortcutsDedupesByPrefix(): void {
  const input = JSON.stringify([
    { prefix: "gh", commandId: "first" },
    { prefix: "gh", commandId: "second" },
    { prefix: "fb", commandId: "feedback" },
    { prefix: "fb", commandId: "other" },
    { prefix: "zz", commandId: "unique" },
  ]);
  const result = parseShortcuts(input, SHORTCUTS_DEFAULT);
  assertEquals(result.length, 3, "duplicate prefixes should be deduped");
  assertEquals(result[0].prefix, "gh", "first 'gh' entry is retained");
  assertEquals(result[0].commandId, "first", "first occurrence wins");
  assertEquals(result[1].prefix, "fb", "first 'fb' entry is retained");
  assertEquals(result[1].commandId, "feedback", "first occurrence wins");
  assertEquals(result[2].prefix, "zz", "unique entry is kept");
}

/**
 * Verifies the defaultVal argument propagates: a distinct default is returned
 * verbatim on rejection (so callers can pass [] vs a non-empty fallback and
 * observe the difference).
 */
function testParseShortcutsDefaultValPropagates(): void {
  const custom: CommandPaletteShortcut[] = [
    { prefix: "a", commandId: "b" },
    { prefix: "c", commandId: "d" },
  ];
  const result = parseShortcuts("garbage", custom);
  assertEquals(
    result.length,
    2,
    "rejection should yield the custom defaultVal length",
  );
  assertEquals(result[0].prefix, "a", "custom defaultVal[0] propagated");
  assertEquals(result[1].prefix, "c", "custom defaultVal[1] propagated");
}

/** Verifies that an empty defaultVal ([]) is honored on rejection. */
function testParseShortcutsEmptyDefaultHonored(): void {
  const result = parseShortcuts("not-json", []);
  assertEquals(result.length, 0, "empty defaultVal should yield []");
}

// ---------------------------------------------------------------------------
// parseSelectableCommands(jsonStr)
// ---------------------------------------------------------------------------
//
// `parseSelectableCommands` is the chrome-side strict parser for the
// `floorp.commandPalette.selectableCommands` pref (the command catalogue the
// settings page reads). It always falls back to [] on any malformed input —
// there is no `defaultVal` parameter. Each element must match
// `{id:string, label:string, category:string}`.

/** Verifies a valid two-element array round-trips through the parser. */
function testParseSelectableCommandsValidArray(): void {
  const input = JSON.stringify([
    { id: "cmd-1", label: "Command One", category: "tools" },
    { id: "cmd-2", label: "Command Two", category: "navigation" },
  ]);
  const result = parseSelectableCommands(input);
  assertEquals(result.length, 2, "valid array should yield 2 entries");
  assertEquals(result[0].id, "cmd-1", "first id preserved");
  assertEquals(result[0].label, "Command One", "first label preserved");
  assertEquals(result[0].category, "tools", "first category preserved");
}

/** Verifies an empty JSON array is accepted. */
function testParseSelectableCommandsEmptyArray(): void {
  const result = parseSelectableCommands("[]");
  assertEquals(result.length, 0, "empty JSON array should yield 0 entries");
}

/** Verifies an empty input string falls back to []. */
function testParseSelectableCommandsEmptyStringFallsBack(): void {
  const result = parseSelectableCommands("");
  assertEquals(result.length, 0, "empty string should yield []");
}

/** Verifies a literal "null" JSON value falls back to []. */
function testParseSelectableCommandsJsonNullFallsBack(): void {
  const result = parseSelectableCommands("null");
  assertEquals(result.length, 0, "'null' is not an array -> []");
}

/** Verifies invalid JSON falls back to []. */
function testParseSelectableCommandsInvalidJsonFallsBack(): void {
  const result = parseSelectableCommands("not-json{");
  assertEquals(result.length, 0, "invalid JSON should yield []");
}

/** Verifies valid JSON that is NOT an array falls back to []. */
function testParseSelectableCommandsNonArrayFallsBack(): void {
  const result = parseSelectableCommands('{"id":"x","label":"y","category":"z"}');
  assertEquals(result.length, 0, "JSON object (non-array) should yield []");
}

/**
 * Verifies the strict shape guard: a non-string `category` (number) rejects
 * the WHOLE array.
 */
function testParseSelectableCommandsNumericCategoryRejectsAll(): void {
  const input = JSON.stringify([
    { id: "ok", label: "OK", category: "tools" },
    { id: "bad", label: "Bad", category: 5 },
  ]);
  const result = parseSelectableCommands(input);
  assertEquals(
    result.length,
    0,
    "any non-string category should reject the whole array -> []",
  );
}

/**
 * Verifies the strict shape guard: a missing `label` rejects the whole array.
 */
function testParseSelectableCommandsMissingLabelRejectsAll(): void {
  const input = JSON.stringify([{ id: "x", category: "y" }]);
  const result = parseSelectableCommands(input);
  assertEquals(
    result.length,
    0,
    "missing label should reject the whole array -> []",
  );
}

/**
 * Verifies the SelectableCommand type shape is enforced: extra keys are
 * tolerated (the guard only checks the required three keys are strings).
 */
function testParseSelectableCommandsExtraKeysTolerated(): void {
  const input = JSON.stringify([
    { id: "x", label: "Y", category: "z", extra: "ignored", n: 1 },
  ]);
  const result = parseSelectableCommands(input);
  assertEquals(result.length, 1, "extra keys should not reject a valid entry");
  assertEquals(result[0].id, "x", "id preserved with extra keys present");
}

/** Verifies a null element inside the array rejects the whole array. */
function testParseSelectableCommandsNullElementRejectsAll(): void {
  const input = JSON.stringify([
    { id: "x", label: "Y", category: "z" },
    null,
  ]);
  const result = parseSelectableCommands(input);
  assertEquals(
    result.length,
    0,
    "a null element should reject the whole array -> []",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

const tests: TestCase[] = [
  // clampInt
  { name: "clampInt returns in-range value unchanged", fn: testClampIntReturnsInRangeValue },
  { name: "clampInt clamps below min", fn: testClampIntClampsBelowMin },
  { name: "clampInt clamps above max", fn: testClampIntClampsAboveMax },
  { name: "clampInt keeps lower boundary", fn: testClampIntKeepsLowerBoundary },
  { name: "clampInt keeps upper boundary", fn: testClampIntKeepsUpperBoundary },
  { name: "clampInt rounds 560.7 up to 561", fn: testClampIntRoundsFractionalUp },
  { name: "clampInt rounds 560.3 down to 560", fn: testClampIntRoundsFractionalDown },
  { name: "clampInt rounds 2.5 half up to 3", fn: testClampIntRoundsHalfUp },
  { name: "clampInt returns fallback for NaN", fn: testClampIntReturnsFallbackForNaN },
  { name: "clampInt returns fallback for +Infinity", fn: testClampIntReturnsFallbackForPositiveInfinity },
  { name: "clampInt returns fallback for -Infinity", fn: testClampIntReturnsFallbackForNegativeInfinity },
  { name: "clampInt preserves negative in-range value", fn: testClampIntPreservesNegativeInRange },
  { name: "clampInt works with real WIDTH_BOUNDS (560)", fn: testClampIntWorksWithRealWidthBounds },
  { name: "clampInt clamps oversize value via MAX_HEIGHT_BOUNDS", fn: testClampIntWorksWithRealMaxHeightBounds },
  { name: "clampInt clamps negative offsetTop via OFFSET_TOP_BOUNDS", fn: testClampIntWorksWithRealOffsetTopBounds },
  { name: "clampInt works with real FONT_SIZE_BOUNDS (14 in-range)", fn: testClampIntWorksWithRealFontSizeBoundsInRange },
  { name: "clampInt clamps below min via FONT_SIZE_BOUNDS (5 -> 11)", fn: testClampIntClampsBelowMinViaFontSizeBounds },
  { name: "clampInt clamps above max via FONT_SIZE_BOUNDS (40 -> 22)", fn: testClampIntClampsAboveMaxViaFontSizeBounds },
  { name: "clampInt keeps lower boundary via FONT_SIZE_BOUNDS (11)", fn: testClampIntKeepsLowerBoundaryViaFontSizeBounds },
  { name: "clampInt keeps upper boundary via FONT_SIZE_BOUNDS (22)", fn: testClampIntKeepsUpperBoundaryViaFontSizeBounds },
  // normalizeHorizontalAlign
  { name: "normalizeHorizontalAlign keeps 'center'", fn: testNormalizeHorizontalAlignCenter },
  { name: "normalizeHorizontalAlign keeps 'left'", fn: testNormalizeHorizontalAlignLeft },
  { name: "normalizeHorizontalAlign keeps 'right'", fn: testNormalizeHorizontalAlignRight },
  { name: "normalizeHorizontalAlign falls back for invalid string", fn: testNormalizeHorizontalAlignInvalidString },
  { name: "normalizeHorizontalAlign falls back for empty string", fn: testNormalizeHorizontalAlignEmptyString },
  { name: "normalizeHorizontalAlign rejects uppercase 'CENTER'", fn: testNormalizeHorizontalAlignUppercaseRejected },
  { name: "normalizeHorizontalAlign falls back for 'top'", fn: testNormalizeHorizontalAlignTopFallsBack },
  // parseShortcuts
  { name: "parseShortcuts returns valid two-element array", fn: testParseShortcutsValidArray },
  { name: "parseShortcuts returns single-element array", fn: testParseShortcutsSingleElement },
  { name: "parseShortcuts accepts empty JSON array", fn: testParseShortcutsEmptyArray },
  { name: "parseShortcuts falls back to defaultVal for empty string", fn: testParseShortcutsEmptyStringFallsBack },
  { name: "parseShortcuts falls back to defaultVal for 'null'", fn: testParseShortcutsJsonNullFallsBack },
  { name: "parseShortcuts falls back to defaultVal for invalid JSON", fn: testParseShortcutsInvalidJsonFallsBack },
  { name: "parseShortcuts falls back to defaultVal for non-array JSON", fn: testParseShortcutsNonArrayFallsBack },
  { name: "parseShortcuts rejects whole array when one prefix is numeric", fn: testParseShortcutsNumericPrefixRejectsAll },
  { name: "parseShortcuts rejects whole array when commandId is missing", fn: testParseShortcutsMissingCommandIdRejectsAll },
  { name: "parseShortcuts rejects whole array when commandId is numeric", fn: testParseShortcutsNumericCommandIdRejectsAll },
  { name: "parseShortcuts propagates a custom defaultVal", fn: testParseShortcutsDefaultValPropagates },
  { name: "parseShortcuts dedupes duplicate prefixes (first wins)", fn: testParseShortcutsDedupesByPrefix },
  { name: "parseShortcuts honors an empty defaultVal", fn: testParseShortcutsEmptyDefaultHonored },
  // parseSelectableCommands
  { name: "parseSelectableCommands returns valid two-element array", fn: testParseSelectableCommandsValidArray },
  { name: "parseSelectableCommands accepts empty JSON array", fn: testParseSelectableCommandsEmptyArray },
  { name: "parseSelectableCommands falls back to [] for empty string", fn: testParseSelectableCommandsEmptyStringFallsBack },
  { name: "parseSelectableCommands falls back to [] for 'null'", fn: testParseSelectableCommandsJsonNullFallsBack },
  { name: "parseSelectableCommands falls back to [] for invalid JSON", fn: testParseSelectableCommandsInvalidJsonFallsBack },
  { name: "parseSelectableCommands falls back to [] for non-array JSON", fn: testParseSelectableCommandsNonArrayFallsBack },
  { name: "parseSelectableCommands rejects whole array when one category is numeric", fn: testParseSelectableCommandsNumericCategoryRejectsAll },
  { name: "parseSelectableCommands rejects whole array when label is missing", fn: testParseSelectableCommandsMissingLabelRejectsAll },
  { name: "parseSelectableCommands tolerates extra keys on valid entries", fn: testParseSelectableCommandsExtraKeysTolerated },
  { name: "parseSelectableCommands rejects whole array when an element is null", fn: testParseSelectableCommandsNullElementRejectsAll },
];

export function runAllTests(): void {
  runTests("config.test.ts", tests);
}
