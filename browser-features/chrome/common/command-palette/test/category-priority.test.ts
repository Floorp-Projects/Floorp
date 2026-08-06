// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  assertStringArrayEqual,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import {
  DEFAULT_CATEGORY_PRIORITY,
  compareByPriority,
  getCategoryPriorityIndex,
  parseCategoryPriority,
  sortCategoriesByPriority,
  truncateByCategory,
} from "../category-priority.ts";

/**
 * Unit tests for the pure category-priority helpers.
 *
 * These functions are pure (no browser APIs), so the tests only use the
 * assertion utilities from the harness and would also run under
 * `deno task test:host` if pointed at this file. They are tagged
 * `@colocated-env browser` to match the rest of the command-palette suite.
 */

// ---------------------------------------------------------------------------
// DEFAULT_CATEGORY_PRIORITY sanity
// ---------------------------------------------------------------------------

/** Verifies the exported default has the documented length and bookends. */
function testDefaultCategoryPriorityShape(): void {
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

// ---------------------------------------------------------------------------
// getCategoryPriorityIndex
// ---------------------------------------------------------------------------

/** Verifies that index 0 (the head of the list) is returned for the leader. */
function testGetCategoryPriorityIndexReturnsZeroForHead(): void {
  assertEquals(
    getCategoryPriorityIndex("navigation", DEFAULT_CATEGORY_PRIORITY),
    0,
    "head category should have priority index 0",
  );
}

/** Verifies that the tail entry gets `length - 1`. */
function testGetCategoryPriorityIndexReturnsLastIndexForTail(): void {
  assertEquals(
    getCategoryPriorityIndex(
      "bookmark-suggestions",
      DEFAULT_CATEGORY_PRIORITY,
    ),
    DEFAULT_CATEGORY_PRIORITY.length - 1,
    "tail category should have priority index length-1",
  );
}

/** Verifies that a mid-list category returns its actual position. */
function testGetCategoryPriorityIndexReturnsMidPosition(): void {
  assertEquals(
    getCategoryPriorityIndex("workspace", DEFAULT_CATEGORY_PRIORITY),
    12,
    "'workspace' should sit at index 12 in the default list",
  );
}

/** Verifies that an unknown category sinks with MAX_SAFE_INTEGER. */
function testGetCategoryPriorityIndexSinksUnknownCategory(): void {
  assertEquals(
    getCategoryPriorityIndex("does-not-exist", DEFAULT_CATEGORY_PRIORITY),
    Number.MAX_SAFE_INTEGER,
    "unknown category should return MAX_SAFE_INTEGER so it sorts last",
  );
}

/** Verifies behavior on an empty priority list — everything is unknown. */
function testGetCategoryPriorityIndexHandlesEmptyList(): void {
  assertEquals(
    getCategoryPriorityIndex("navigation", []),
    Number.MAX_SAFE_INTEGER,
    "empty priority list should treat every category as unknown",
  );
}

// ---------------------------------------------------------------------------
// compareByPriority
// ---------------------------------------------------------------------------

/** Verifies a lower-indexed category sorts before a higher-indexed one. */
function testCompareByPriorityLowerIndexFirst(): void {
  const list = ["tabs", "navigation"];
  // navigation (idx 1) - tabs (idx 0) => +1 > 0  -> a should come AFTER b
  assert(
    compareByPriority({ category: "navigation" }, { category: "tabs" }, list) >
      0,
    "navigation has higher index than tabs in this list, so diff > 0",
  );
}

/** Verifies a higher-indexed category sorts after a lower-indexed one. */
function testCompareByPriorityHigherIndexLater(): void {
  const list = ["tabs", "navigation"];
  // tabs (idx 0) - navigation (idx 1) => -1 < 0 -> a should come BEFORE b
  assert(
    compareByPriority({ category: "tabs" }, { category: "navigation" }, list) <
      0,
    "tabs has lower index than navigation in this list, so diff < 0",
  );
}

/** Verifies that two items with the same category produce a 0 comparator. */
function testCompareByPriorityEqualCategoryReturnsZero(): void {
  assertEquals(
    compareByPriority({ category: "tabs" }, { category: "tabs" }, ["tabs"]),
    0,
    "same category should produce comparator 0",
  );
}

/** Verifies that two unknown categories also produce 0 (MAX - MAX). */
function testCompareByPriorityBothUnknownReturnsZero(): void {
  assertEquals(
    compareByPriority(
      { category: "x" },
      { category: "y" },
      DEFAULT_CATEGORY_PRIORITY,
    ),
    0,
    "two unknown categories share MAX_SAFE_INTEGER, so diff is 0",
  );
}

/** Verifies that an unknown category sorts after a known one. */
function testCompareByPriorityUnknownSortsLast(): void {
  assert(
    compareByPriority(
      { category: "zzz-unknown" },
      { category: "navigation" },
      DEFAULT_CATEGORY_PRIORITY,
    ) > 0,
    "unknown category should compare as later than a known one",
  );
}

// ---------------------------------------------------------------------------
// parseCategoryPriority
// ---------------------------------------------------------------------------

/** Verifies a valid JSON string array is parsed and returned by value. */
function testParseCategoryPriorityValidArray(): void {
  const parsed = parseCategoryPriority('["tabs","navigation"]');
  assertEquals(parsed.length, 2, "should parse a 2-element array");
  assertEquals(parsed[0], "tabs", "first element should be 'tabs'");
  assertEquals(parsed[1], "navigation", "second element should be 'navigation'");
}

/** Verifies that `null` falls back to a copy of the default list. */
function testParseCategoryPriorityNullFallsBackToDefault(): void {
  const parsed = parseCategoryPriority(null);
  assertStringArrayEqual(
    parsed,
    [...DEFAULT_CATEGORY_PRIORITY],
    "null input should return default",
  );
}

/** Verifies that `undefined` falls back to a copy of the default list. */
function testParseCategoryPriorityUndefinedFallsBackToDefault(): void {
  const parsed = parseCategoryPriority(undefined);
  assertStringArrayEqual(
    parsed,
    [...DEFAULT_CATEGORY_PRIORITY],
    "undefined input should return default",
  );
}

/** Verifies that the empty string falls back to a copy of the default list. */
function testParseCategoryPriorityEmptyStringFallsBackToDefault(): void {
  const parsed = parseCategoryPriority("");
  assertStringArrayEqual(
    parsed,
    [...DEFAULT_CATEGORY_PRIORITY],
    "empty string should return default",
  );
}

/** Verifies that invalid JSON falls back to a copy of the default list. */
function testParseCategoryPriorityInvalidJsonFallsBackToDefault(): void {
  const parsed = parseCategoryPriority("not json");
  assertStringArrayEqual(
    parsed,
    [...DEFAULT_CATEGORY_PRIORITY],
    "invalid JSON should return default",
  );
}

/** Verifies that valid-JSON non-array (string) falls back to default. */
function testParseCategoryPriorityJsonStringFallsBackToDefault(): void {
  const parsed = parseCategoryPriority('"hello"');
  assertStringArrayEqual(
    parsed,
    [...DEFAULT_CATEGORY_PRIORITY],
    "JSON string (non-array) should return default",
  );
}

/** Verifies that valid-JSON object falls back to default. */
function testParseCategoryPriorityJsonObjectFallsBackToDefault(): void {
  const parsed = parseCategoryPriority('{"a":1}');
  assertStringArrayEqual(
    parsed,
    [...DEFAULT_CATEGORY_PRIORITY],
    "JSON object should return default",
  );
}

/** Verifies that an array containing a non-string element falls back. */
function testParseCategoryPriorityNonStringElementFallsBackToDefault(): void {
  const parsed = parseCategoryPriority('["a",123]');
  assertStringArrayEqual(
    parsed,
    [...DEFAULT_CATEGORY_PRIORITY],
    "array with non-string element should return default",
  );
}

/** Verifies that an array containing `null` falls back to default. */
function testParseCategoryPriorityNullElementFallsBackToDefault(): void {
  const parsed = parseCategoryPriority('["a",null]');
  assertStringArrayEqual(
    parsed,
    [...DEFAULT_CATEGORY_PRIORITY],
    "array with null element should return default",
  );
}

/**
 * Verifies that a valid empty JSON array is returned as-is (NOT default).
 *
 * This is intentional: `parsed.every(el => typeof el === "string")` is
 * vacuously true for `[]`, so the helper trusts the user's explicit choice
 * to clear the list. Documenting this as behavior, not a bug.
 */
function testParseCategoryPriorityEmptyArrayReturnedAsIs(): void {
  const parsed = parseCategoryPriority("[]");
  assertEquals(parsed.length, 0, "valid empty array should be returned as-is");
}

/** Verifies the helper returns a fresh array each call (no shared reference). */
function testParseCategoryPriorityReturnsFreshArray(): void {
  const a = parseCategoryPriority(null);
  const b = parseCategoryPriority(null);
  assert(a !== b, "each call should return a new array instance");
  assert(
    a !== DEFAULT_CATEGORY_PRIORITY,
    "returned array should not be the readonly default reference",
  );
  // Mutating one must not affect the other or the default.
  a.push("mutated");
  assertEquals(b.length, DEFAULT_CATEGORY_PRIORITY.length, "mutation of one result should not leak to another");
}

// ---------------------------------------------------------------------------
// sortCategoriesByPriority
// ---------------------------------------------------------------------------

/** Verifies the sort order matches the priority list, with unknowns sinking. */
function testSortCategoriesByPriorityOrdersByPriority(): void {
  const items = [
    { category: "history-suggestions" },
    { category: "navigation" },
    { category: "switcher" },
    { category: "tabs" },
  ];
  const sorted = sortCategoriesByPriority(items, DEFAULT_CATEGORY_PRIORITY);
  assertEquals(sorted.length, 4, "sort should preserve item count");
  assertEquals(sorted[0].category, "navigation", "navigation should be first");
  assertEquals(sorted[1].category, "tabs", "tabs should be second");
  assertEquals(sorted[2].category, "switcher", "switcher should be third");
  assertEquals(
    sorted[3].category,
    "history-suggestions",
    "history-suggestions should be last",
  );
}

/** Verifies that unknown categories all sink to the bottom in input order. */
function testSortCategoriesByPriorityUnknownsSinkInInputOrder(): void {
  const items = [
    { category: "unknown-b" },
    { category: "navigation" },
    { category: "unknown-a" },
  ];
  const sorted = sortCategoriesByPriority(items, DEFAULT_CATEGORY_PRIORITY);
  assertEquals(sorted[0].category, "navigation", "known category should float to top");
  assertEquals(sorted[1].category, "unknown-b", "first unknown should keep input order");
  assertEquals(sorted[2].category, "unknown-a", "second unknown should keep input order");
}

/** Verifies that the input array is NOT mutated. */
function testSortCategoriesByPriorityDoesNotMutateInput(): void {
  const items = [
    { category: "history-suggestions" },
    { category: "navigation" },
  ];
  const snapshotBefore = items.map((x) => x.category);
  sortCategoriesByPriority(items, DEFAULT_CATEGORY_PRIORITY);
  assertStringArrayEqual(
    items.map((x) => x.category),
    snapshotBefore,
    "input array should be unchanged after sort",
  );
}

/** Verifies that the returned array is a new instance (not the input). */
function testSortCategoriesByPriorityReturnsNewArray(): void {
  const items = [{ category: "navigation" }];
  const sorted = sortCategoriesByPriority(items, DEFAULT_CATEGORY_PRIORITY);
  assert(sorted !== items, "should return a new array, not the input reference");
}

/** Verifies stable sort: equal-priority items keep their incoming order. */
function testSortCategoriesByPriorityIsStable(): void {
  // All three are unknown to the default list — they share MAX_SAFE_INTEGER.
  const items = [
    { category: "unknown1", id: 1 },
    { category: "unknown2", id: 2 },
    { category: "unknown3", id: 3 },
  ];
  const sorted = sortCategoriesByPriority(items, DEFAULT_CATEGORY_PRIORITY);
  assertEquals(sorted[0].id, 1, "stable sort preserves order: id 1 first");
  assertEquals(sorted[1].id, 2, "stable sort preserves order: id 2 second");
  assertEquals(sorted[2].id, 3, "stable sort preserves order: id 3 third");
}

/** Verifies that an empty input produces an empty output. */
function testSortCategoriesByPriorityEmptyInput(): void {
  const sorted = sortCategoriesByPriority([], DEFAULT_CATEGORY_PRIORITY);
  assertEquals(sorted.length, 0, "empty input should produce empty output");
}

/** Verifies that an empty priority list leaves items in input order. */
function testSortCategoriesByPriorityEmptyPriorityList(): void {
  const items = [
    { category: "z" },
    { category: "a" },
    { category: "m" },
  ];
  const sorted = sortCategoriesByPriority(items, []);
  // All categories are unknown against an empty list, so stability wins.
  assertEquals(sorted[0].category, "z", "empty priority list keeps input order (z)");
  assertEquals(sorted[1].category, "a", "empty priority list keeps input order (a)");
  assertEquals(sorted[2].category, "m", "empty priority list keeps input order (m)");
}

// ---------------------------------------------------------------------------
// truncateByCategory
// ---------------------------------------------------------------------------

/**
 * Verifies the per-category truncation behavior: keeps at most `limit` items
 * per category, preserves incoming order, never mutates the input, and treats
 * non-finite / non-positive limits as unlimited (defensive).
 */
function testTruncateByCategory(): void {
  // Basic truncation: 3 categories with varying counts, limit=2
  const items = [
    { category: "a", id: 1 },
    { category: "a", id: 2 },
    { category: "a", id: 3 }, // dropped (3rd in "a")
    { category: "b", id: 4 },
    { category: "b", id: 5 },
    { category: "b", id: 6 }, // dropped (3rd in "b")
    { category: "b", id: 7 }, // dropped (4th in "b")
    { category: "c", id: 8 }, // kept (only 1 in "c")
  ];
  const truncated = truncateByCategory(items, 2);
  // Expected: a,a,b,b,c (5 items), ids 1,2,4,5,8
  assertEquals(truncated.length, 5, "should drop items beyond limit per category");
  assertEquals(truncated[0].id, 1, "first a kept");
  assertEquals(truncated[1].id, 2, "second a kept");
  assertEquals(truncated[2].id, 4, "first b kept");
  assertEquals(truncated[3].id, 5, "second b kept");
  assertEquals(truncated[4].id, 8, "only c kept");

  // Does NOT mutate the input
  assertEquals(items.length, 8, "input not mutated");
  assertEquals(items[2].id, 3, "input items unchanged");

  // Order preservation within each category (incoming order kept)
  const ordered = [
    { category: "x", n: "first" },
    { category: "y", n: "alpha" },
    { category: "x", n: "second" },
    { category: "y", n: "beta" },
    { category: "x", n: "third" },
  ];
  const t1 = truncateByCategory(ordered, 1);
  assertEquals(t1.length, 2, "limit=1 keeps 1 per category");
  assertEquals(t1[0].n, "first", "first x by incoming order");
  assertEquals(t1[1].n, "alpha", "first y by incoming order");

  // Limit larger than any category → no truncation
  const small = [
    { category: "a", id: 1 },
    { category: "b", id: 2 },
  ];
  const noTrunc = truncateByCategory(small, 100);
  assertEquals(noTrunc.length, 2, "limit > max category size: no items dropped");

  // Empty input → empty output
  assertEquals(truncateByCategory([], 5).length, 0, "empty input");

  // limit = 0 → unlimited (returns shallow copy)
  const zero = truncateByCategory(items, 0);
  assertEquals(zero.length, items.length, "limit=0 unlimited");
  assert(zero !== items, "limit=0 still returns a new array");

  // limit negative → unlimited (defensive)
  const neg = truncateByCategory(items, -3);
  assertEquals(neg.length, items.length, "negative limit unlimited");

  // limit = NaN → unlimited (defensive, Number.isFinite check)
  const nan = truncateByCategory(items, NaN);
  assertEquals(nan.length, items.length, "NaN limit unlimited");

  // limit = Infinity → effectively unlimited
  const inf = truncateByCategory(items, Infinity);
  assertEquals(inf.length, items.length, "Infinity limit unlimited");

  // limit fractional (e.g. 2.9) → floored to 2
  const frac = truncateByCategory(items, 2.9);
  assertEquals(frac.length, 5, "fractional limit floored");

  // Single category, all dropped beyond limit
  const singleCat = [
    { category: "only", id: 1 },
    { category: "only", id: 2 },
    { category: "only", id: 3 },
    { category: "only", id: 4 },
  ];
  const sc = truncateByCategory(singleCat, 2);
  assertEquals(sc.length, 2, "single category truncated");
  assertEquals(sc[0].id, 1, "first kept");
  assertEquals(sc[1].id, 2, "second kept");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

const tests: TestCase[] = [
  // DEFAULT_CATEGORY_PRIORITY shape
  { name: "DEFAULT_CATEGORY_PRIORITY has 19 entries with correct bookends", fn: testDefaultCategoryPriorityShape },
  // getCategoryPriorityIndex
  { name: "getCategoryPriorityIndex returns 0 for the head category", fn: testGetCategoryPriorityIndexReturnsZeroForHead },
  { name: "getCategoryPriorityIndex returns length-1 for the tail category", fn: testGetCategoryPriorityIndexReturnsLastIndexForTail },
  { name: "getCategoryPriorityIndex returns mid position for a known category", fn: testGetCategoryPriorityIndexReturnsMidPosition },
  { name: "getCategoryPriorityIndex returns MAX_SAFE_INTEGER for unknown category", fn: testGetCategoryPriorityIndexSinksUnknownCategory },
  { name: "getCategoryPriorityIndex returns MAX_SAFE_INTEGER for empty list", fn: testGetCategoryPriorityIndexHandlesEmptyList },
  // compareByPriority
  { name: "compareByPriority returns positive for lower-index category as `a`", fn: testCompareByPriorityLowerIndexFirst },
  { name: "compareByPriority returns negative for higher-index category as `a`", fn: testCompareByPriorityHigherIndexLater },
  { name: "compareByPriority returns 0 for equal categories", fn: testCompareByPriorityEqualCategoryReturnsZero },
  { name: "compareByPriority returns 0 when both categories are unknown", fn: testCompareByPriorityBothUnknownReturnsZero },
  { name: "compareByPriority places unknown categories after known ones", fn: testCompareByPriorityUnknownSortsLast },
  // parseCategoryPriority
  { name: "parseCategoryPriority parses a valid JSON string array", fn: testParseCategoryPriorityValidArray },
  { name: "parseCategoryPriority(null) returns a copy of the default", fn: testParseCategoryPriorityNullFallsBackToDefault },
  { name: "parseCategoryPriority(undefined) returns a copy of the default", fn: testParseCategoryPriorityUndefinedFallsBackToDefault },
  { name: "parseCategoryPriority('') returns a copy of the default", fn: testParseCategoryPriorityEmptyStringFallsBackToDefault },
  { name: "parseCategoryPriority('not json') returns a copy of the default", fn: testParseCategoryPriorityInvalidJsonFallsBackToDefault },
  { name: "parseCategoryPriority('\"hello\"') returns a copy of the default", fn: testParseCategoryPriorityJsonStringFallsBackToDefault },
  { name: "parseCategoryPriority('{\"a\":1}') returns a copy of the default", fn: testParseCategoryPriorityJsonObjectFallsBackToDefault },
  { name: "parseCategoryPriority('[\"a\",123]') returns a copy of the default", fn: testParseCategoryPriorityNonStringElementFallsBackToDefault },
  { name: "parseCategoryPriority('[\"a\",null]') returns a copy of the default", fn: testParseCategoryPriorityNullElementFallsBackToDefault },
  { name: "parseCategoryPriority('[]') is returned as-is (NOT default)", fn: testParseCategoryPriorityEmptyArrayReturnedAsIs },
  { name: "parseCategoryPriority returns a fresh array each call", fn: testParseCategoryPriorityReturnsFreshArray },
  // sortCategoriesByPriority
  { name: "sortCategoriesByPriority orders items by priority list", fn: testSortCategoriesByPriorityOrdersByPriority },
  { name: "sortCategoriesByPriority sinks unknown categories in input order", fn: testSortCategoriesByPriorityUnknownsSinkInInputOrder },
  { name: "sortCategoriesByPriority does not mutate its input", fn: testSortCategoriesByPriorityDoesNotMutateInput },
  { name: "sortCategoriesByPriority returns a new array reference", fn: testSortCategoriesByPriorityReturnsNewArray },
  { name: "sortCategoriesByPriority is stable for equal-priority items", fn: testSortCategoriesByPriorityIsStable },
  { name: "sortCategoriesByPriority([]) returns []", fn: testSortCategoriesByPriorityEmptyInput },
  { name: "sortCategoriesByPriority with empty priority list keeps input order", fn: testSortCategoriesByPriorityEmptyPriorityList },
  // truncateByCategory
  { name: "truncateByCategory keeps at most `limit` per category without mutating input", fn: testTruncateByCategory },
];

export function runAllTests(): void {
  runTests("category-priority.test.ts", tests);
}
