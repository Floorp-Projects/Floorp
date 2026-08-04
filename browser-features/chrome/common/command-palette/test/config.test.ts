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
  WIDTH_BOUNDS,
  MAX_HEIGHT_BOUNDS,
  OFFSET_TOP_BOUNDS,
} from "../config.ts";

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
  // normalizeHorizontalAlign
  { name: "normalizeHorizontalAlign keeps 'center'", fn: testNormalizeHorizontalAlignCenter },
  { name: "normalizeHorizontalAlign keeps 'left'", fn: testNormalizeHorizontalAlignLeft },
  { name: "normalizeHorizontalAlign keeps 'right'", fn: testNormalizeHorizontalAlignRight },
  { name: "normalizeHorizontalAlign falls back for invalid string", fn: testNormalizeHorizontalAlignInvalidString },
  { name: "normalizeHorizontalAlign falls back for empty string", fn: testNormalizeHorizontalAlignEmptyString },
  { name: "normalizeHorizontalAlign rejects uppercase 'CENTER'", fn: testNormalizeHorizontalAlignUppercaseRejected },
  { name: "normalizeHorizontalAlign falls back for 'top'", fn: testNormalizeHorizontalAlignTopFallsBack },
];

export function runAllTests(): void {
  runTests("config.test.ts", tests);
}
