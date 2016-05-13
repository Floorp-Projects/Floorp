/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the breadcrumbs widget content is correct.

const TEST_URI = URL_ROOT + "doc_inspector_breadcrumbs.html";
const NODES = [
  {selector: "#i1111", result: "i1 i11 i111 i1111"},
  {selector: "#i22", result: "i2 i22"},
  {selector: "#i2111", result: "i2 i21 i211 i2111"},
  {selector: "#i21", result: "i2 i21 i211 i2111"},
  {selector: "#i22211", result: "i2 i22 i222 i2221 i22211"},
  {selector: "#i22", result: "i2 i22 i222 i2221 i22211"},
  {selector: "#i3", result: "i3"},
];

add_task(function* () {
  let { inspector } = yield openInspectorForURL(TEST_URI);
  let container = inspector.panelDoc.getElementById("inspector-breadcrumbs");

  for (let node of NODES) {
    info("Testing node " + node.selector);

    info("Selecting node and waiting for breadcrumbs to update");
    let breadcrumbsUpdated = inspector.once("breadcrumbs-updated");
    yield selectNode(node.selector, inspector);
    yield breadcrumbsUpdated;

    info("Performing checks for node " + node.selector);
    let buttonsLabelIds = node.result.split(" ");

    // html > body > …
    is(container.childNodes.length, buttonsLabelIds.length + 2,
      "Node " + node.selector + ": Items count");

    for (let i = 2; i < container.childNodes.length; i++) {
      let expectedId = "#" + buttonsLabelIds[i - 2];
      let button = container.childNodes[i];
      let labelId = button.querySelector(".breadcrumbs-widget-item-id");
      is(labelId.textContent, expectedId,
        "Node #" + node.selector + ": button " + i + " matches");
    }

    let checkedButton = container.querySelector("button[checked]");
    let labelId = checkedButton.querySelector(".breadcrumbs-widget-item-id");
    let id = inspector.selection.nodeFront.id;
    is(labelId.textContent, "#" + id,
      "Node #" + node.selector + ": selection matches");
  }

  yield testPseudoElements(inspector, container);
});

function* testPseudoElements(inspector, container) {
  info("Checking for pseudo elements");

  let pseudoParent = yield getNodeFront("#pseudo-container", inspector);
  let children = yield inspector.walker.children(pseudoParent);
  is(children.nodes.length, 2, "Pseudo children returned from walker");

  let beforeElement = children.nodes[0];
  let breadcrumbsUpdated = inspector.once("breadcrumbs-updated");
  yield selectNode(beforeElement, inspector);
  yield breadcrumbsUpdated;
  is(container.childNodes[3].textContent, "::before",
     "::before shows up in breadcrumb");

  let afterElement = children.nodes[1];
  breadcrumbsUpdated = inspector.once("breadcrumbs-updated");
  yield selectNode(afterElement, inspector);
  yield breadcrumbsUpdated;
  is(container.childNodes[3].textContent, "::after",
     "::before shows up in breadcrumb");
}
