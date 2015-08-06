/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler categories are mapped correctly.
 */

function run_test() {
  run_next_test();
}

add_task(function () {
  let global = require("devtools/performance/global");
  let l10n = global.L10N;
  let categories = global.CATEGORIES;
  let mappings = global.CATEGORY_MAPPINGS;
  let count = categories.length;

  ok(count,
    "Should have a non-empty list of categories available.");

  ok(categories.some(e => e.color),
    "All categories have an associated color.");

  ok(categories.every(e => e.label),
    "All categories have an associated label.");

  ok(categories.every(e => e.label === l10n.getStr("category." + e.abbrev)),
    "All categories have a correctly localized label.");

  ok(Object.keys(mappings).every(e => (Number(e) >= 9000 && Number(e) <= 9999) || Number.isInteger(Math.log2(e))),
    "All bitmask mappings keys are powers of 2, or between 9000-9999 for special categories.");

  ok(Object.keys(mappings).every(e => categories.indexOf(mappings[e]) !== -1),
    "All bitmask mappings point to a category.");
});
