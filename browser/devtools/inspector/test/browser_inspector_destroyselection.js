/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test to ensure inspector can handle destruction of selected node inside an
// iframe.

const TEST_URL = TEST_URL_ROOT + "browser_inspector_destroyselection.html";

let test = asyncTest(function* () {
  let { inspector } = yield openInspectorForURL(TEST_URL);

  let iframe = getNode("iframe");
  let node = getNode("span", { document: iframe.contentDocument });
  yield selectNode(node, inspector);

  info("Removing iframe.");
  iframe.remove();

  let lh = new LayoutHelpers(window.content);
  ok(!lh.isNodeConnected(node), "Node considered as disconnected.");
  ok(!inspector.selection.isConnected(), "Selection considered as disconnected.");

  yield inspector.once("inspector-updated");
});
