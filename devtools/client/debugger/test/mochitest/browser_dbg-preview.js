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

  await testPreviews(dbg, "multipleTokens", [
    { line: 81, column: 4, expression: "foo", result: "Object" },
    { line: 81, column: 11, expression: "blip", result: "Object" },
    { line: 82, column: 8, expression: "bar", result: "Object" },
    { line: 84, column: 16, expression: "boom", result: `0` },
  ]);

  await testHoveringInvalidTargetTokens(dbg);

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

  await testMovingFromATokenToAnother(dbg);
});

async function testPreviews(dbg, fnName, previews) {
  invokeInTab(fnName);
  await waitForPaused(dbg);

  await assertPreviews(dbg, previews);
  await resume(dbg);

  info(`Ran tests for ${fnName}`);
}

async function testHoveringInvalidTargetTokens(dbg) {
  // Test hovering tokens for which we shouldn't have a preview popup displayed
  invokeInTab("invalidTargets");
  await waitForPaused(dbg);
  // CodeMirror refreshes after inline previews are displayed, so wait until they're rendered.
  await waitForInlinePreviews(dbg);

  await assertNoPreviews(dbg, `"a"`, 69, 4);
  await assertNoPreviews(dbg, `false`, 70, 4);
  await assertNoPreviews(dbg, `undefined`, 71, 4);
  await assertNoPreviews(dbg, `null`, 72, 4);
  await assertNoPreviews(dbg, `42`, 73, 4);
  await assertNoPreviews(dbg, `const`, 74, 4);

  // checking inline preview widget
  // Move the cursor to the top left corner to have a clean state
  resetCursorPositionToTopLeftCorner(dbg);

  const inlinePreviewEl = findElementWithSelector(
    dbg,
    ".CodeMirror-code .CodeMirror-widget"
  );
  is(inlinePreviewEl.innerText, `myVar:"foo"`, "got expected inline preview");

  const racePromise = Promise.any([
    waitForElement(dbg, "previewPopup"),
    wait(500).then(() => "TIMEOUT"),
  ]);
  // Hover over the inline preview element
  hoverToken(inlinePreviewEl);
  const raceResult = await racePromise;
  is(raceResult, "TIMEOUT", "No popup was displayed over the inline preview");

  await resume(dbg);
}

async function assertNoPreviews(dbg, expression, line, column) {
  // Move the cursor to the top left corner to have a clean state
  resetCursorPositionToTopLeftCorner(dbg);

  // Hover the token
  const result = await Promise.race([
    tryHoverTokenAtLine(dbg, expression, line, column, "previewPopup"),
    wait(500).then(() => "TIMEOUT"),
  ]);
  is(result, "TIMEOUT", `No popup was displayed when hovering "${expression}"`);
}

function resetCursorPositionToTopLeftCorner(dbg) {
  EventUtils.synthesizeMouse(
    findElement(dbg, "codeMirror"),
    0,
    0,
    {
      type: "mousemove",
    },
    dbg.win
  );
}

async function testMovingFromATokenToAnother(dbg) {
  info(
    "Check that moving the mouse to another token when popup is displayed updates highlighted token and popup position"
  );
  invokeInTab("classPreview");
  await waitForPaused(dbg);

  info("Hover token `Foo` in `Foo.#privateStatic` expression");
  const fooTokenEl = getTokenElAtLine(dbg, "Foo", 50, 44);
  const cm = getCM(dbg);
  const onScrolled = waitForScrolling(cm);
  cm.scrollIntoView({ line: 49, ch: 0 }, 0);
  await onScrolled;
  const { element: fooPopupEl } = await tryHoverToken(dbg, fooTokenEl, "popup");
  ok(!!fooPopupEl, "popup is displayed");
  ok(
    fooTokenEl.classList.contains("preview-token"),
    "`Foo` token is highlighted"
  );

  // store original position
  const originalPopupPosition = fooPopupEl.getBoundingClientRect().x;

  info(
    "Move mouse over the `#privateStatic` token in `Foo.#privateStatic` expression"
  );
  const privateStaticTokenEl = getTokenElAtLine(dbg, "#privateStatic", 50, 48);

  // The sequence of event to trigger the bug this is covering isn't easily reproducible
  // by firing a few chosen events (because of React async rendering), so we are going to
  // mimick moving the mouse from the `Foo` to `#privateStatic` in a given amount of time

  // So get all the different token quads to compute their center
  const fooTokenQuad = fooTokenEl.getBoxQuads()[0];
  const privateStaticTokenQuad = privateStaticTokenEl.getBoxQuads()[0];
  const fooXCenter =
    fooTokenQuad.p1.x + (fooTokenQuad.p2.x - fooTokenQuad.p1.x) / 2;
  const fooYCenter =
    fooTokenQuad.p1.y + (fooTokenQuad.p3.y - fooTokenQuad.p1.y) / 2;
  const privateStaticXCenter =
    privateStaticTokenQuad.p1.x +
    (privateStaticTokenQuad.p2.x - privateStaticTokenQuad.p1.x) / 2;
  const privateStaticYCenter =
    privateStaticTokenQuad.p1.y +
    (privateStaticTokenQuad.p3.y - privateStaticTokenQuad.p1.y) / 2;

  // we can then compute the distance to cover between the two token centers
  const xDistance = privateStaticXCenter - fooXCenter;
  const yDistance = privateStaticYCenter - fooYCenter;
  const movementDuration = 50;
  const xIncrements = xDistance / movementDuration;
  const yIncrements = yDistance / movementDuration;

  // Finally, we're going to fire a mouseover event every ms
  info("Move mousecursor between the `Foo` token to the `#privateStatic` one");
  for (let i = 0; i < movementDuration; i++) {
    const x = fooXCenter + (yDistance + i * xIncrements);
    const y = fooYCenter + (yDistance + i * yIncrements);
    EventUtils.synthesizeMouseAtPoint(
      x,
      y,
      {
        type: "mouseover",
      },
      fooTokenEl.ownerGlobal
    );
    await wait(1);
  }

  info("Wait for the popup to display the data for `#privateStatic`");
  await waitFor(() => {
    const popup = findElement(dbg, "popup");
    if (!popup) {
      return false;
    }
    // for `Foo`, the header text content is "Foo", so when it's "Object", we know the
    // popup was updated
    return (
      popup.querySelector(".preview-popup .node .objectBox")?.textContent ===
      "Object"
    );
  });
  ok(true, "Popup is displayed for #privateStatic");

  ok(
    !fooTokenEl.classList.contains("preview-token"),
    "`Foo` token is not highlighted anymore"
  );
  ok(
    privateStaticTokenEl.classList.contains("preview-token"),
    "`#privateStatic` token is highlighted"
  );

  const privateStaticPopupEl = await waitForElement(dbg, "popup");
  const newPopupPosition = privateStaticPopupEl.getBoundingClientRect().x;
  isnot(
    Math.round(newPopupPosition),
    Math.round(originalPopupPosition),
    `Popup position was updated`
  );

  await resume(dbg);
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
