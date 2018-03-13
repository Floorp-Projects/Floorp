/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the box model displays the right values for positions.

const TEST_URI = `
  <style type='text/css'>
    div {
      position: absolute;
      left: 0;
      margin: 0;
      padding: 0;
      display: none;
      height: 100px;
      width: 100px;
      border: 10px solid black;
    }
  </style>
  <div>Test Node</div>
`;

// Expected values:
const res1 = [
  {
    selector: ".boxmodel-position.boxmodel-top > span",
    value: "auto"
  },
  {
    selector: ".boxmodel-position.boxmodel-right > span",
    value: "auto"
  },
  {
    selector: ".boxmodel-position.boxmodel-bottom > span",
    value: "auto"
  },
  {
    selector: ".boxmodel-position.boxmodel-left > span",
    value: 0
  },
];

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openBoxModelView();
  let node = yield getNodeFront("div", inspector);
  let children = yield inspector.markup.walker.children(node);
  let beforeElement = children.nodes[0];

  yield selectNode(beforeElement, inspector);
  yield testPositionValues(inspector, view);
});

function* testPositionValues(inspector, view) {
  info("Test that the position values of the box model are correct");
  let viewdoc = view.document;

  for (let i = 0; i < res1.length; i++) {
    let elt = viewdoc.querySelector(res1[i].selector);
    is(elt.textContent, res1[i].value,
       res1[i].selector + " has the right value.");
  }
}
