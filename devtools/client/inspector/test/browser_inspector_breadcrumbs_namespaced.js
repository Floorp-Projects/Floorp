/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the breadcrumbs widget content for namespaced elements is correct.

const XHTML = `
  <!DOCTYPE html>
  <html xmlns="http://www.w3.org/1999/xhtml"
        xmlns:svg="http://www.w3.org/2000/svg">
    <body>
      <svg:svg width="100" height="100">
        <svg:clipPath id="clip">
          <svg:rect id="rectangle" x="0" y="0" width="10" height="5"></svg:rect>
        </svg:clipPath>
        <svg:circle cx="0" cy="0" r="5"></svg:circle>
      </svg:svg>
    </body>
  </html>
`;

const TEST_URI = "data:application/xhtml+xml;charset=utf-8," + encodeURI(XHTML);

const NODES = [
  {selector: "clipPath", nodes: ["svg:svg", "svg:clipPath"],
    nodeName: "svg:clipPath", title: "svg:clipPath#clip"},
  {selector: "circle", nodes: ["svg:svg", "svg:circle"],
    nodeName: "svg:circle", title: "svg:circle"},
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

    let checkedButton = container.querySelector("button[checked]");

    let labelTag = checkedButton.querySelector(".breadcrumbs-widget-item-tag");
    is(labelTag.textContent, node.nodeName,
      "Node " + node.selector + " has the expected tag name");

    is(checkedButton.getAttribute("tooltiptext"), node.title,
      "Node " + node.selector + " has the expected tooltip");
  }
});
