/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the profiler categories are mapped correctly.
 */

function run_test() {
  run_next_test();
}

add_task(function () {
  let { CATEGORIES, CATEGORY_MAPPINGS } = require("devtools/client/performance/modules/categories");
  let { L10N } = require("devtools/client/performance/modules/global");
  let count = CATEGORIES.length;

  ok(count,
    "Should have a non-empty list of categories available.");

  ok(CATEGORIES.some(e => e.color),
    "All categories have an associated color.");

  ok(CATEGORIES.every(e => e.label),
    "All categories have an associated label.");

  ok(CATEGORIES.every(e => e.label === L10N.getStr("category." + e.abbrev)),
    "All categories have a correctly localized label.");

  ok(Object.keys(CATEGORY_MAPPINGS).every(e => (Number(e) >= 9000 && Number(e) <= 9999) ||
                                                Number.isInteger(Math.log2(e))),
    "All bitmask mappings keys are powers of 2, or between 9000-9999 for special " +
    "categories.");

  ok(Object.keys(CATEGORY_MAPPINGS).every(e => CATEGORIES.indexOf(CATEGORY_MAPPINGS[e])
                                               !== -1),
    "All bitmask mappings point to a category.");
});
