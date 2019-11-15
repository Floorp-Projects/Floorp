/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that users can inspect objects logged from cross-domain iframes -
// bug 869003.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-inspect-cross-domain-objects-top.html";

add_task(async function() {
  requestLongerTimeout(2);

  // Bug 1518138: GC heuristics are broken for this test, so that the test
  // ends up running out of memory. Try to work-around the problem by GCing
  // before the test begins.
  Cu.forceShrinkingGC();

  const hud = await openNewTabAndConsole(
    "data:text/html;charset=utf8,<p>hello"
  );

  info("Wait for the 'foobar' message to be logged by the frame");
  const onMessage = waitForMessage(hud, "foobar");
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URI);
  const { node } = await onMessage;

  const objectInspectors = [...node.querySelectorAll(".tree")];
  is(
    objectInspectors.length,
    3,
    "There is the expected number of object inspectors"
  );

  const [oi1, oi2, oi3] = objectInspectors;

  info("Expanding the first object inspector");
  await expandObjectInspector(oi1);

  // The first object inspector now looks like:
  // ▼ {…}
  // |  bug: 869003
  // |  hello: "world!"
  // |  ▶︎ <prototype>: Object { … }

  const oi1Nodes = oi1.querySelectorAll(".node");
  is(oi1Nodes.length, 4, "There is the expected number of nodes in the tree");
  ok(oi1.textContent.includes("bug: 869003"), "Expected content");
  ok(oi1.textContent.includes('hello: "world!"'), "Expected content");

  info("Expanding the second object inspector");
  await expandObjectInspector(oi2);

  // The second object inspector now looks like:
  // ▼ func()
  // |  arguments: null
  // |  bug: 869003
  // |  caller: null
  // |  hello: "world!"
  // |  length: 1
  // |  name: "func"
  // |  ▶︎ prototype: Object { … }
  // |  ▶︎ <prototype>: function ()

  const oi2Nodes = oi2.querySelectorAll(".node");
  is(oi2Nodes.length, 9, "There is the expected number of nodes in the tree");
  ok(oi2.textContent.includes("arguments: null"), "Expected content");
  ok(oi2.textContent.includes("bug: 869003"), "Expected content");
  ok(oi2.textContent.includes("caller: null"), "Expected content");
  ok(oi2.textContent.includes('hello: "world!"'), "Expected content");
  ok(oi2.textContent.includes("length: 1"), "Expected content");
  ok(oi2.textContent.includes('name: "func"'), "Expected content");

  info(
    "Check that the logged element can be highlighted and clicked to jump to inspector"
  );
  const toolbox = hud.toolbox;
  // Loading the inspector panel at first, to make it possible to listen for
  // new node selections
  await toolbox.loadTool("inspector");

  const elementNode = oi3.querySelector(".objectBox-node");
  ok(elementNode !== null, "Node was logged as expected");
  const view = node.ownerDocument.defaultView;

  info("Highlight the node by moving the cursor on it");
  // the inspector should be initialized first and then the node should
  // highlight after the hover effect.
  const inspectorFront = await toolbox.target.getFront("inspector");
  const onNodeHighlight = inspectorFront.highlighter.once("node-highlight");

  EventUtils.synthesizeMouseAtCenter(elementNode, { type: "mousemove" }, view);

  await onNodeHighlight;
  ok(true, "Highlighter is displayed");
  // Move the mouse out of the node to prevent failure when test is run multiple times.
  EventUtils.synthesizeMouseAtCenter(oi1, { type: "mousemove" }, view);

  const openInInspectorIcon = elementNode.querySelector(".open-inspector");
  ok(openInInspectorIcon !== null, "There is an open in inspector icon");

  info(
    "Clicking on the inspector icon and waiting for the inspector to be selected"
  );
  const onNewNode = toolbox.selection.once("new-node-front");
  openInInspectorIcon.click();
  const inspectorSelectedNodeFront = await onNewNode;

  ok(true, "Inspector selected and new node got selected");
  is(inspectorSelectedNodeFront.id, "testEl", "The expected node was selected");
});

function expandObjectInspector(oi) {
  const onMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  return onMutation;
}
