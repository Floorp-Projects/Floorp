/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test hovering on an object, which will show a popup and on a
// simple value, which will show a tooltip.

"use strict";

// Showing/hiding the preview tooltip can be slow as we wait for CodeMirror scroll...
requestLongerTimeout(2);

add_task(async function () {
  const dbg = await initDebugger("doc-preview.html", "preview.js");

  await testPreviews(dbg, "testInline", [
    { line: 17, column: 16, expression: "prop", result: 2 },
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
    { line: 27, column: 10, expression: "empty", result: "Object" },
    { line: 28, column: 22, expression: "foo", result: 1 },
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
    { line: 50, column: 20, expression: "x", result: 1 },
    { line: 50, column: 29, expression: "#privateVar", result: 2 },
    {
      line: 50,
      column: 47,
      expression: "#privateStatic",
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
    { line: 51, column: 39, expression: "#privateVar", result: 2 },
  ]);

  info(
    "Check that closing the preview tooltip doesn't release the underlying object actor"
  );
  invokeInTab("classPreview");
  await waitForPaused(dbg);
  info("Display popup a first time and hide it");
  await assertPreviews(dbg, [
    {
      line: 60,
      column: 7,
      expression: "y",
      fields: [["hello", "{…}"]],
    },
  ]);

  info("Display the popup again and try to expand a property");
  const { element: popupEl, tokenEl } = await tryHovering(
    dbg,
    60,
    7,
    "previewPopup"
  );
  const nodes = popupEl.querySelectorAll(".preview-popup .node");
  const initialNodesLength = nodes.length;
  nodes[1].querySelector(".arrow").click();
  await waitFor(
    () =>
      popupEl.querySelectorAll(".preview-popup .node").length >
      initialNodesLength
  );
  ok(true, `"hello" was expanded`);
  await closePreviewForToken(dbg, tokenEl, "popup");
  await resume(dbg);
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
  const { element: popupEl, tokenEl } = await tryHovering(
    dbg,
    34,
    10,
    "previewPopup"
  );

  info("Wait for top level node to expand and child nodes to load");
  await waitUntil(
    () => popupEl.querySelectorAll(".preview-popup .node").length > 1
  );

  const oiNodes = Array.from(popupEl.querySelectorAll(".preview-popup .node"));

  const displayedPropertyNames = oiNodes.map(
    oiNode => oiNode.querySelector(".object-label")?.textContent
  );
  Assert.deepEqual(displayedPropertyNames, [
    null, // No property name is displayed for the root node
    "[0…99]",
    "[100…100]",
    "length",
    "<prototype>",
  ]);
  const node = oiNodes.find(
    oiNode => oiNode.querySelector(".object-label")?.textContent === "length"
  );
  if (!node) {
    ok(false, `The "length" property is not displayed in the popup`);
  } else {
    is(
      node.querySelector(".objectBox").textContent,
      "101",
      `The "length" property has the expected value`
    );
  }
  await closePreviewForToken(dbg, tokenEl, "popup");

  await resume(dbg);
}
