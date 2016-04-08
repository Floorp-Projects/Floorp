/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Tests that selecting a node using the browser context menu (inspect element)
// or the element picker focuses that node so that the keyboard can be used
// immediately.

const TEST_URL = "data:text/html;charset=utf8,<div>test element</div>";

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);

  info("Select the test node with the browser ctx menu");
  yield clickOnInspectMenuItem(testActor, "div");
  assertNodeSelected(inspector, "div");

  info("Press arrowUp to focus <body> " +
       "(which works if the node was focused properly)");
  yield selectPreviousNodeWithArrowUp(inspector);
  assertNodeSelected(inspector, "body");

  info("Select the test node with the element picker");
  yield selectWithElementPicker(inspector, testActor);
  assertNodeSelected(inspector, "div");

  info("Press arrowUp to focus <body> " +
       "(which works if the node was focused properly)");
  yield selectPreviousNodeWithArrowUp(inspector);
  assertNodeSelected(inspector, "body");
});

function assertNodeSelected(inspector, tagName) {
  is(inspector.selection.nodeFront.tagName.toLowerCase(), tagName,
    `The <${tagName}> node is selected`);
}

function selectPreviousNodeWithArrowUp(inspector) {
  let onNodeHighlighted = inspector.toolbox.once("node-highlight");
  let onUpdated = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("VK_UP", {});
  return Promise.all([onUpdated, onNodeHighlighted]);
}

function* selectWithElementPicker(inspector, testActor) {
  yield startPicker(inspector.toolbox);

  yield BrowserTestUtils.synthesizeMouseAtCenter("div", {
    type: "mousemove",
  }, gBrowser.selectedBrowser);

  yield testActor.synthesizeKey({key: "VK_RETURN", options: {}});
  yield inspector.once("inspector-updated");
}
