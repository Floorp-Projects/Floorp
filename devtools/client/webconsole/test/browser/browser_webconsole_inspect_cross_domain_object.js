/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that users can inspect objects logged from cross-domain iframes -
// bug 869003.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/" +
  "test-inspect-cross-domain-objects-top.html";

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
    2,
    "There is the expected number of object inspectors"
  );

  const [oi1, oi2] = objectInspectors;

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
});

function expandObjectInspector(oi) {
  const onMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  return onMutation;
}
