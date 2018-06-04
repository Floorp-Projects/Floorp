/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the profiler categories are mapped correctly.
 */

add_task(function() {
  const { CATEGORIES } = require("devtools/client/performance/modules/categories");
  const { L10N } = require("devtools/client/performance/modules/global");
  const count = CATEGORIES.length;

  ok(count,
    "Should have a non-empty list of categories available.");

  ok(CATEGORIES.some(e => e.color),
    "All categories have an associated color.");

  ok(CATEGORIES.every(e => e.label),
    "All categories have an associated label.");

  ok(CATEGORIES.every(e => e.label === L10N.getStr("category." + e.abbrev)),
    "All categories have a correctly localized label.");
});
