/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the breadcrumbs widget content is correct.

const TEST_URI = URL_ROOT + "doc_inspector_breadcrumbs.html";
const NODES = [
  {selector: "#i1111", ids: "i1 i11 i111 i1111", nodeName: "div",
    title: "div#i1111"},
  {selector: "#i22", ids: "i2 i22", nodeName: "div",
    title: "div#i22"},
  {selector: "#i2111", ids: "i2 i21 i211 i2111", nodeName: "div",
    title: "div#i2111"},
  {selector: "#i21", ids: "i2 i21 i211 i2111", nodeName: "div",
    title: "div#i21"},
  {selector: "#i22211", ids: "i2 i22 i222 i2221 i22211", nodeName: "div",
    title: "div#i22211"},
  {selector: "#i22", ids: "i2 i22 i222 i2221 i22211", nodeName: "div",
    title: "div#i22"},
  {selector: "#i3", ids: "i3", nodeName: "article",
    title: "article#i3"},
  {selector: "clipPath", ids: "vector clip", nodeName: "clipPath",
    title: "clipPath#clip"},
];

add_task(function* () {
  let { inspector } = yield openInspectorForURL(TEST_URI);
  let breadcrumbs = inspector.panelDoc.getElementById("inspector-breadcrumbs");
  let container = breadcrumbs.querySelector(".html-arrowscrollbox-inner");

  for (let node of NODES) {
    info("Testing node " + node.selector);

    info("Selecting node and waiting for breadcrumbs to update");
    let breadcrumbsUpdated = inspector.once("breadcrumbs-updated");
    yield selectNode(node.selector, inspector);
    yield breadcrumbsUpdated;

    info("Performing checks for node " + node.selector);
    let buttonsLabelIds = node.ids.split(" ");

    // html > body > …
    is(container.childNodes.length, buttonsLabelIds.length + 2,
      "Node " + node.selector + ": Items count");

    for (let i = 2; i < container.childNodes.length; i++) {
      let expectedId = "#" + buttonsLabelIds[i - 2];
      let button = container.childNodes[i];
      let labelId = button.querySelector(".breadcrumbs-widget-item-id");
      is(labelId.textContent, expectedId,
        "Node " + node.selector + ": button " + i + " matches");
    }

    let checkedButton = container.querySelector("button[checked]");
    let labelId = checkedButton.querySelector(".breadcrumbs-widget-item-id");
    let id = inspector.selection.nodeFront.id;
    is(labelId.textContent, "#" + id,
      "Node " + node.selector + ": selection matches");

    let labelTag = checkedButton.querySelector(".breadcrumbs-widget-item-tag");
    is(labelTag.textContent, node.nodeName,
      "Node " + node.selector + " has the expected tag name");

    is(checkedButton.getAttribute("title"), node.title,
      "Node " + node.selector + " has the expected tooltip");
  }

  yield testPseudoElements(inspector, container);
  yield testComments(inspector, container);
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

function* testComments(inspector, container) {
  info("Checking for comment elements");

  let breadcrumbs = inspector.breadcrumbs;
  let checkedButtonIndex = 2;
  let button = container.childNodes[checkedButtonIndex];

  let onBreadcrumbsUpdated = inspector.once("breadcrumbs-updated");
  button.click();
  yield onBreadcrumbsUpdated;

  is(breadcrumbs.currentIndex, checkedButtonIndex, "New button is selected");
  ok(breadcrumbs.outer.hasAttribute("aria-activedescendant"),
    "Active descendant must be set");

  let comment = [...inspector.markup._containers].find(([node]) =>
    node.nodeType === Ci.nsIDOMNode.COMMENT_NODE)[0];

  let onInspectorUpdated = inspector.once("inspector-updated");
  inspector.selection.setNodeFront(comment);
  yield onInspectorUpdated;

  is(breadcrumbs.currentIndex, -1,
    "When comment is selected no breadcrumb should be checked");
  ok(!breadcrumbs.outer.hasAttribute("aria-activedescendant"),
    "Active descendant must not be set");

  onInspectorUpdated = inspector.once("inspector-updated");
  onBreadcrumbsUpdated = inspector.once("breadcrumbs-updated");
  button.click();
  yield Promise.all([onInspectorUpdated, onBreadcrumbsUpdated]);

  is(breadcrumbs.currentIndex, checkedButtonIndex,
    "Same button is selected again");
  ok(breadcrumbs.outer.hasAttribute("aria-activedescendant"),
    "Active descendant must be set again");
}
