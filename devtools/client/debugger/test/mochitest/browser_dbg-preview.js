/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test hovering on an object, which will show a popup and on a
// simple value, which will show a tooltip.
add_task(async function() {
  const dbg = await initDebugger("doc-preview.html", "preview.js");

  await previews(dbg, "testInline", [
    { line: 17, column: 16, expression: "obj?.prop", result: 2 },
  ]);

  await selectSource(dbg, "preview.js");
  await testBucketedArray(dbg);

  await previews(dbg, "empties", [
    { line: 6, column: 9, expression: "a", result: '""' },
    { line: 7, column: 9, expression: "b", result: "false" },
    { line: 8, column: 9, expression: "c", result: "undefined" },
    { line: 9, column: 9, expression: "d", result: "null" },
  ]);

  await previews(dbg, "objects", [
    { line: 27, column: 10, expression: "empty", result: "No properties" },
    { line: 28, column: 22, expression: "obj?.foo", result: 1 },
  ]);

  await previews(dbg, "smalls", [
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
});

async function previews(dbg, fnName, previews) {
  const invokeResult = invokeInTab(fnName);
  await waitForPaused(dbg);

  await assertPreviews(dbg, previews);
  await resume(dbg);

  info(`Ran tests for ${fnName}`);
}

async function testBucketedArray(dbg) {
  const invokeResult = invokeInTab("largeArray");
  await waitForPaused(dbg);
  const preview = await hoverOnToken(dbg, 34, 10, "popup");

  is(
    preview.properties.map(p => p.name).join(" "),
    "[0…99] [100…100] length <prototype>",
    "Popup properties are bucketed"
  );

  is(preview.properties[0].meta.endIndex, 99, "first bucket ends at 99");
  is(preview.properties[2].contents.value, 101, "length is 101");
  await resume(dbg);
}
