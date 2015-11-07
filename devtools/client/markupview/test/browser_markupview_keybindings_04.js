/* vim: set ts=2 et sw=2 tw=80: */
/* global nsContextMenu*/
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that selecting a node using the browser context menu (inspect element)
// or the element picker focuses that node so that the keyboard can be used
// immediately.

const TEST_URL = "data:text/html;charset=utf8,<div>test element</div>";

add_task(function*() {
  let {toolbox, inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Select the test node with the browser ctx menu");
  yield selectWithBrowserMenu(inspector);
  assertNodeSelected(inspector, "div");

  info("Press arrowUp to focus <body> " +
       "(which works if the node was focused properly)");
  let onNodeHighlighted = toolbox.once("node-highlight");
  EventUtils.synthesizeKey("VK_UP", {});
  yield waitForChildrenUpdated(inspector);
  yield onNodeHighlighted;
  assertNodeSelected(inspector, "body");

  info("Select the test node with the element picker");
  yield selectWithElementPicker(inspector);
  assertNodeSelected(inspector, "div");

  info("Press arrowUp to focus <body> " +
       "(which works if the node was focused properly)");
  onNodeHighlighted = toolbox.once("node-highlight");
  EventUtils.synthesizeKey("VK_UP", {});
  yield waitForChildrenUpdated(inspector);
  yield onNodeHighlighted;
  assertNodeSelected(inspector, "body");
});

function assertNodeSelected(inspector, tagName) {
  is(inspector.selection.nodeFront.tagName.toLowerCase(), tagName,
    `The <${tagName}> node is selected`);
}

function* selectWithBrowserMenu(inspector) {
  yield BrowserTestUtils.synthesizeMouseAtCenter("div", {
    type: "contextmenu",
    button: 2
  }, gBrowser.selectedBrowser);

  // nsContextMenu also requires the popupNode to be set, but we can't set it to
  // node under e10s as it's a CPOW, not a DOM node. But under e10s,
  // nsContextMenu won't use the property anyway, so just try/catching is ok.
  try {
    document.popupNode = getNode("div");
  } catch (e) {}

  let contentAreaContextMenu = document.querySelector("#contentAreaContextMenu");
  let contextMenu = new nsContextMenu(contentAreaContextMenu);
  yield contextMenu.inspectNode();

  contentAreaContextMenu.hidden = true;
  contentAreaContextMenu.hidePopup();
  contextMenu.hiding();

  yield inspector.once("inspector-updated");
}

function* selectWithElementPicker(inspector) {
  yield inspector.toolbox.highlighterUtils.startPicker();

  yield BrowserTestUtils.synthesizeMouseAtCenter("div", {
    type: "mousemove",
  }, gBrowser.selectedBrowser);

  executeInContent("Test:SynthesizeKey", {
    key: "VK_RETURN",
    options: {}
  }, {}, false);
  yield inspector.once("inspector-updated");
}
