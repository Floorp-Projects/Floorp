/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that once a node has been hovered over and marked as such, if it is
// navigated away using the keyboard, the highlighter moves to the new node, and
// if it is then navigated back to, it is briefly highlighted again

const TEST_PAGE = "data:text/html;charset=utf-8," +
                  "<p id=\"one\">one</p><p id=\"two\">two</p>";

let test = asyncTest(function*() {
  let {inspector} = yield addTab(TEST_PAGE).then(openInspector);

  info("Making sure the markup-view frame is focused");
  inspector.markup._frame.focus();

  // Mock the highlighter to easily track which node gets highlighted.
  // We don't need to test here that the highlighter is actually visible, we
  // just care about whether the markup-view asks it to be shown
  let highlightedNode = null;
  inspector.toolbox._highlighter.showBoxModel = function(nodeFront) {
    highlightedNode = nodeFront;
    return promise.resolve();
  };
  inspector.toolbox._highlighter.hideBoxModel = function() {
    return promise.resolve();
  };

  function isHighlighting(node, desc) {
    is(highlightedNode, getContainerForRawNode(node, inspector).node, desc);
  }

  info("Hover over <p#one> line in the markup-view");
  yield hoverContainer("#one", inspector);
  isHighlighting(getNode("#one"), "<p#one> is highlighted");

  info("Navigate to <p#two> with the keyboard");
  let onUpdated = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("VK_DOWN", {});
  yield onUpdated;
  let onUpdated = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("VK_DOWN", {});
  yield onUpdated;
  isHighlighting(getNode("#two"), "<p#two> is highlighted");

  info("Navigate back to <p#one> with the keyboard");
  let onUpdated = inspector.once("inspector-updated");
  EventUtils.synthesizeKey("VK_UP", {});
  yield onUpdated;
  isHighlighting(getNode("#one"), "<p#one> is highlighted again");
});
