/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test hovering on an object, which will show a popup and on a
// simple value, which will show a tooltip.

"use strict";

add_task(async function() {
  const dbg = await initDebugger("doc-preview.html", "preview.js");

  await testPreviews(dbg, "testInline", [
    { line: 17, column: 16, expression: "obj?.prop", result: 2 },
  ]);

  await selectSource(dbg, "preview.js");
  await testBucketedArray(dbg);

  await testPreviews(dbg, "empties", [
    { line: 6, column: 9, expression: "a", result: '""' },
    { line: 7, column: 9, expression: "b", result: "false" },
    { line: 8, column: 9, expression: "c", result: "undefined" },
    { line: 9, column: 9, expression: "d", result: "null" },
  ]);

  await testPreviews(dbg, "objects", [
    { line: 27, column: 10, expression: "empty", result: "No properties" },
    { line: 28, column: 22, expression: "obj?.foo", result: 1 },
  ]);

  await testPreviews(dbg, "smalls", [
    { line: 14, column: 9, expression: "a", result: '"..."' },
    { line: 15, column: 9, expression: "b", result: "true" },
    { line: 16, column: 9, expression: "c", result: "1" },
    {
      line: 17,
      column: 9,
      expression: "d",
      fields: [["length", "0"]],
    },
  ]);

  await testPreviews(dbg, "classPreview", [
    { line: 50, column: 20, expression: "this.x", result: 1 },
    { line: 50, column: 29, expression: "this.#privateVar", result: 2 },
    {
      line: 50,
      column: 47,
      expression: "Foo.#privateStatic",
      fields: [
        ["first", `"a"`],
        ["second", `"b"`],
      ],
    },
    {
      line: 51,
      column: 26,
      expression: "this",
      fields: [
        ["x", "1"],
        ["#privateVar", "2"],
      ],
    },
    { line: 51, column: 39, expression: "this.#privateVar", result: 2 },
  ]);
});

async function testPreviews(dbg, fnName, previews) {
  invokeInTab(fnName);
  await waitForPaused(dbg);

  await assertPreviews(dbg, previews);
  await resume(dbg);

  info(`Ran tests for ${fnName}`);
}

async function testBucketedArray(dbg) {
  invokeInTab("largeArray");
  await waitForPaused(dbg);
  await tryHovering(dbg, 34, 10, "popup");
  const preview = dbg.selectors.getPreview();

  is(
    preview.properties.map(p => p.name).join(" "),
    "[0…99] [100…100] length <prototype>",
    "Popup properties are bucketed"
  );

  is(preview.properties[0].meta.endIndex, 99, "first bucket ends at 99");
  is(preview.properties[2].contents.value, 101, "length is 101");
  await resume(dbg);
}
