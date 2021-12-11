/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Tests that selecting a node using the browser context menu (inspect element)
// or the element picker focuses that node so that the keyboard can be used
// immediately.

const TEST_URL = "data:text/html;charset=utf8,<div>test element</div>";

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);

  info("Select the test node with the browser ctx menu");
  await clickOnInspectMenuItem("div");
  assertNodeSelected(inspector, "div");

  info(
    "Press arrowUp to focus <body> " +
      "(which works if the node was focused properly)"
  );
  await selectPreviousNodeWithArrowUp(inspector);
  assertNodeSelected(inspector, "body");

  info("Select the test node with the element picker");
  await selectWithElementPicker(inspector);
  assertNodeSelected(inspector, "div");

  info(
    "Press arrowUp to focus <body> " +
      "(which works if the node was focused properly)"
  );
  await selectPreviousNodeWithArrowUp(inspector);
  assertNodeSelected(inspector, "body");
});

function assertNodeSelected(inspector, tagName) {
  is(
    inspector.selection.nodeFront.tagName.toLowerCase(),
    tagName,
    `The <${tagName}> node is selected`
  );
}

function selectPreviousNodeWithArrowUp(inspector) {
  const { waitForHighlighterTypeShown } = getHighlighterTestHelpers(inspector);
  const onNodeHighlighted = waitForHighlighterTypeShown(
    inspector.highlighters.TYPES.BOXMODEL
  );
  const onUpdated = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  return Promise.all([onUpdated, onNodeHighlighted]);
}

async function selectWithElementPicker(inspector) {
  await startPicker(inspector.toolbox);

  await safeSynthesizeMouseEventAtCenterInContentPage(
    "div",
    {
      type: "mousemove",
    },
    gBrowser.selectedBrowser
  );

  BrowserTestUtils.synthesizeKey("KEY_Enter", {}, gBrowser.selectedBrowser);
  await inspector.once("inspector-updated");
}
