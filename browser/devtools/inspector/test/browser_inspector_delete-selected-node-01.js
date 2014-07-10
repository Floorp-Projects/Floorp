/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test to ensure inspector handles deletion of selected node correctly.

const TEST_URL = TEST_URL_ROOT + "doc_inspector_delete-selected-node-01.html";

let test = asyncTest(function* () {
  let { inspector } = yield openInspectorForURL(TEST_URL);
  let iframe = getNode("iframe");
  let span = getNode("span", { document: iframe.contentDocument });

  yield selectNode(span, inspector);

  info("Removing selected <span> element.");
  let parentNode = span.parentNode;
  span.remove();

  let lh = new LayoutHelpers(window.content);
  ok(!lh.isNodeConnected(span), "Node considered as disconnected.");

  // Wait for the inspector to process the mutation
  yield inspector.once("inspector-updated");
  is(inspector.selection.node, parentNode,
    "Parent node of selected <span> got selected.");
});
