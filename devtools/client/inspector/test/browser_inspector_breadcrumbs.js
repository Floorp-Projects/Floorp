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

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URI);
  const breadcrumbs = inspector.panelDoc.getElementById("inspector-breadcrumbs");
  const container = breadcrumbs.querySelector(".html-arrowscrollbox-inner");

  for (const node of NODES) {
    info("Testing node " + node.selector);

    info("Selecting node and waiting for breadcrumbs to update");
    const breadcrumbsUpdated = inspector.once("breadcrumbs-updated");
    await selectNode(node.selector, inspector);
    await breadcrumbsUpdated;

    info("Performing checks for node " + node.selector);
    const buttonsLabelIds = node.ids.split(" ");

    // html > body > …
    is(container.childNodes.length, buttonsLabelIds.length + 2,
      "Node " + node.selector + ": Items count");

    for (let i = 2; i < container.childNodes.length; i++) {
      const expectedId = "#" + buttonsLabelIds[i - 2];
      const button = container.childNodes[i];
      const labelId = button.querySelector(".breadcrumbs-widget-item-id");
      is(labelId.textContent, expectedId,
        "Node " + node.selector + ": button " + i + " matches");
    }

    const checkedButton = container.querySelector("button[checked]");
    const labelId = checkedButton.querySelector(".breadcrumbs-widget-item-id");
    const id = inspector.selection.nodeFront.id;
    is(labelId.textContent, "#" + id,
      "Node " + node.selector + ": selection matches");

    const labelTag = checkedButton.querySelector(".breadcrumbs-widget-item-tag");
    is(labelTag.textContent, node.nodeName,
      "Node " + node.selector + " has the expected tag name");

    is(checkedButton.getAttribute("title"), node.title,
      "Node " + node.selector + " has the expected tooltip");
  }

  await testPseudoElements(inspector, container);
  await testComments(inspector, container);
});

async function testPseudoElements(inspector, container) {
  info("Checking for pseudo elements");

  const pseudoParent = await getNodeFront("#pseudo-container", inspector);
  const children = await inspector.walker.children(pseudoParent);
  is(children.nodes.length, 2, "Pseudo children returned from walker");

  const beforeElement = children.nodes[0];
  let breadcrumbsUpdated = inspector.once("breadcrumbs-updated");
  await selectNode(beforeElement, inspector);
  await breadcrumbsUpdated;
  is(container.childNodes[3].textContent, "::before",
     "::before shows up in breadcrumb");

  const afterElement = children.nodes[1];
  breadcrumbsUpdated = inspector.once("breadcrumbs-updated");
  await selectNode(afterElement, inspector);
  await breadcrumbsUpdated;
  is(container.childNodes[3].textContent, "::after",
     "::before shows up in breadcrumb");
}

async function testComments(inspector, container) {
  info("Checking for comment elements");

  const breadcrumbs = inspector.breadcrumbs;
  const checkedButtonIndex = 2;
  const button = container.childNodes[checkedButtonIndex];

  let onBreadcrumbsUpdated = inspector.once("breadcrumbs-updated");
  button.click();
  await onBreadcrumbsUpdated;

  is(breadcrumbs.currentIndex, checkedButtonIndex, "New button is selected");
  ok(breadcrumbs.outer.hasAttribute("aria-activedescendant"),
    "Active descendant must be set");

  const comment = [...inspector.markup._containers].find(([node]) =>
    node.nodeType === Node.COMMENT_NODE)[0];

  let onInspectorUpdated = inspector.once("inspector-updated");
  inspector.selection.setNodeFront(comment);
  await onInspectorUpdated;

  is(breadcrumbs.currentIndex, -1,
    "When comment is selected no breadcrumb should be checked");
  ok(!breadcrumbs.outer.hasAttribute("aria-activedescendant"),
    "Active descendant must not be set");

  onInspectorUpdated = inspector.once("inspector-updated");
  onBreadcrumbsUpdated = inspector.once("breadcrumbs-updated");
  button.click();
  await Promise.all([onInspectorUpdated, onBreadcrumbsUpdated]);

  is(breadcrumbs.currentIndex, checkedButtonIndex,
    "Same button is selected again");
  ok(breadcrumbs.outer.hasAttribute("aria-activedescendant"),
    "Active descendant must be set again");
}
