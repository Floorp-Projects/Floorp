/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test "Copy color" item of the context menu #1: Test _isColorPopup.

const TEST_URI = `
  <div style="color:rgb(18, 58, 188);margin:0px;background:span[data-color];">
    Test "Copy color" context menu option
  </div>
`;

add_task(async function() {
  // Test is slow on Linux EC2 instances - Bug 1137765
  requestLongerTimeout(2);
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector} = await openInspector();
  await testView("ruleview", inspector);
  await testView("computedview", inspector);
});

async function testView(viewId, inspector) {
  info("Testing " + viewId);

  await inspector.sidebar.select(viewId);
  const view = inspector.getPanel(viewId).view || inspector.getPanel(viewId).computedView;
  await selectNode("div", inspector);

  testIsColorValueNode(view);
  testIsColorPopupOnAllNodes(view);
  await clearCurrentNodeSelection(inspector);
}

/**
 * A function testing that isColorValueNode correctly detects nodes part of
 * color values.
 */
function testIsColorValueNode(view) {
  info("Testing that child nodes of color nodes are detected.");
  const root = rootElement(view);
  const colorNode = root.querySelector("span[data-color]");

  ok(colorNode, "Color node found");
  for (const node of iterateNodes(colorNode)) {
    ok(isColorValueNode(node), "Node is part of color value.");
  }
}

/**
 * A function testing that _isColorPopup returns a correct value for all nodes
 * in the view.
 */
function testIsColorPopupOnAllNodes(view) {
  const root = rootElement(view);
  for (const node of iterateNodes(root)) {
    testIsColorPopupOnNode(view, node);
  }
}

/**
 * Test result of _isColorPopup with given node.
 * @param object view
 *               A CSSRuleView or CssComputedView instance.
 * @param Node node
 *             A node to check.
 */
function testIsColorPopupOnNode(view, node) {
  info("Testing node " + node);
  view.styleDocument.popupNode = node;
  view.contextMenu._colorToCopy = "";

  const result = view.contextMenu._isColorPopup();
  const correct = isColorValueNode(node);

  is(result, correct, "_isColorPopup returned the expected value " + correct);
  is(view.contextMenu._colorToCopy, (correct) ? "rgb(18, 58, 188)" : "",
     "_colorToCopy was set to the expected value");
}

/**
 * Check if a node is part of color value i.e. it has parent with a 'data-color'
 * attribute.
 */
function isColorValueNode(node) {
  let container = (node.nodeType == node.TEXT_NODE) ?
                   node.parentElement : node;

  const isColorNode = el => el.dataset && "color" in el.dataset;

  while (!isColorNode(container)) {
    container = container.parentNode;
    if (!container) {
      info("No color. Node is not part of color value.");
      return false;
    }
  }

  info("Found a color. Node is part of color value.");

  return true;
}

/**
 * A generator that iterates recursively trough all child nodes of baseNode.
 */
function* iterateNodes(baseNode) {
  yield baseNode;

  for (const child of baseNode.childNodes) {
    yield* iterateNodes(child);
  }
}

/**
 * Returns the root element for the given view, rule or computed.
 */
var rootElement = view => (view.element) ? view.element : view.styleDocument;
