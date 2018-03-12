/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE_URL = URL_ROOT + "page_array.html";
const TEST_ARRAY = [
  "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
  "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z"
];

/**
 * Basic test that checks content of the DOM panel.
 */
add_task(function* () {
  info("Test DOM Panel Array Expansion started");

  let { panel } = yield addTestTab(TEST_PAGE_URL);

  // Expand specified row and wait till children are displayed.
  yield expandRow(panel, "_a");

  // Verify that children is displayed now.
  let childRows = getAllRowsForLabel(panel, "_a");

  let item = childRows.pop();
  is(item.name, "length", "length property is correct");
  is(item.value, 26, "length property value is 26");

  let i = 0;
  for (let name in childRows) {
    let row = childRows[name];

    is(name, i++, `index ${name} is correct and sorted into the correct position`);
    ok(typeof row.name === "number", "array index is displayed as a number");
    is(TEST_ARRAY[name], row.value, `value for array[${name}] is ${row.value}`);
  }
});
