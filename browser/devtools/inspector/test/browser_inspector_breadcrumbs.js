/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the breadcrumbs widget content is correct.

const TEST_URI = TEST_URL_ROOT + "doc_inspector_breadcrumbs.html";
const NODES = [
  {nodeId: "#i1111", result: "i1 i11 i111 i1111"},
  {nodeId: "#i22", result: "i2 i22 i221"},
  {nodeId: "#i2111", result: "i2 i21 i211 i2111"},
  {nodeId: "#i21", result: "i2 i21 i211 i2111"},
  {nodeId: "#i22211", result: "i2 i22 i222 i2221 i22211"},
  {nodeId: "#i22", result: "i2 i22 i222 i2221 i22211"},
];

let test = asyncTest(function*() {
  let { inspector } = yield openInspectorForURL(TEST_URI);
  let container = inspector.panelDoc.getElementById("inspector-breadcrumbs");

  for (let node of NODES) {
    info("Testing node " + node.nodeId);

    let documentNode = getNode(node.nodeId);

    info("Selecting node and waiting for breadcrumbs to update");
    let breadcrumbsUpdated = inspector.once("breadcrumbs-updated");
    let nodeSelected = selectNode(documentNode, inspector);

    yield Promise.all([breadcrumbsUpdated, nodeSelected]);

    info("Performing checks for node " + node.nodeId);
    let buttonsLabelIds = node.result.split(" ");

    // html > body > …
    is(container.childNodes.length, buttonsLabelIds.length + 2,
      "Node " + node.nodeId + ": Items count");

    for (let i = 2; i < container.childNodes.length; i++) {
      let expectedId = "#" + buttonsLabelIds[i - 2];
      let button = container.childNodes[i];
      let labelId = button.querySelector(".breadcrumbs-widget-item-id");
      is(labelId.textContent, expectedId,
        "Node #" + node.nodeId + ": button " + i + " matches");
    }

    let checkedButton = container.querySelector("button[checked]");
    let labelId = checkedButton.querySelector(".breadcrumbs-widget-item-id");
    let id = inspector.selection.node.id;
    is(labelId.textContent, "#" + id,
      "Node #" + node.nodeId + ": selection matches");
  }
});
