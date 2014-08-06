/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler categories are mapped correctly.
 */

function test() {
  let global = devtools.require("devtools/profiler/global");
  let l10n = global.L10N;
  let categories = global.CATEGORIES;
  let mappings = global.CATEGORY_MAPPINGS;
  let count = categories.length;

  ok(count,
    "Should have a non-empty list of categories available.");

  ok(categories.find(e => e.ordinal == count - 1),
    "The maximum category ordinal is the equal to the categories count.");

  is(categories.reduce((a, b) => a + b.ordinal, 0), count * (count - 1) / 2,
    "There is an ordinal for every category in the list.");

  is(categories.filter((e, i, self) => self.find(e => e.ordinal == i)).length, count,
    "All categories have unique ordinals.");

  ok(!categories.some(e => !e.color),
    "All categories have an associated color.");

  ok(!categories.some(e => !e.label),
    "All categories have an associated label.");

  ok(!categories.some(e => e.label != l10n.getStr("category." + e.abbrev)),
    "All categories have a correctly localized label.");

  ok(!Object.keys(mappings).some(e => !Number.isInteger(Math.log2(e))),
    "All bitmask mappings keys are powers of 2.");

  ok(!Object.keys(mappings).some(e => categories.indexOf(mappings[e]) == -1),
    "All bitmask mappings point to a category.");

  finish();
}
