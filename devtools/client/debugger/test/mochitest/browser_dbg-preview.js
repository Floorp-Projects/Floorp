/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

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
  const preview = await hoverOnToken(dbg, 27, 8, "popup");

  is(
    preview.properties.map(p => p.name).join(" "),
    "[0…99] [100…100] length <prototype>",
    "Popup properties are bucketed"
  );

  is(preview.properties[0].meta.endIndex, 99, "first bucket ends at 99");
  is(preview.properties[2].contents.value, 101, "length is 101");
  await resume(dbg);
}

// Test hovering on an object, which will show a popup and on a
// simple value, which will show a tooltip.
add_task(async function() {
  const dbg = await initDebugger("doc-preview.html", "preview.js");
  await selectSource(dbg, "preview.js");

  await testBucketedArray(dbg);

  await previews(dbg, "empties", [
    { line: 2, column: 9, expression: "a", result: '""' },
    { line: 3, column: 9, expression: "b", result: "false" },
    { line: 4, column: 9, expression: "c", result: "undefined" },
    { line: 5, column: 9, expression: "d", result: "null" }
  ]);

  await previews(dbg, "smalls", [
    { line: 10, column: 9, expression: "a", result: '"..."' },
    { line: 11, column: 9, expression: "b", result: "true" },
    { line: 12, column: 9, expression: "c", result: "1" },
    {
      line: 13,
      column: 9,
      expression: "d",
      fields: [["length", "0"]]
    }
  ]);
});
