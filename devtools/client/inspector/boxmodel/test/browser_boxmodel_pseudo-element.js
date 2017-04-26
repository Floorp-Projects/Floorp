/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the box model displays the right values for a pseudo-element.

const TEST_URI = `
  <style type='text/css'>
    div {
      box-sizing: border-box;
      display: block;
      float: left;
      line-height: 20px;
      position: relative;
      z-index: 2;
      height: 100px;
      width: 100px;
      border: 10px solid black;
      padding: 20px;
      margin: 30px auto;
    }

    div::before {
      content: 'before';
      display: block;
      width: 32px;
      height: 32px;
      margin: 0 auto 6px;
    }
  </style>
  <div>Test Node</div>
`;

// Expected values:
const res1 = [
  {
    selector: ".boxmodel-element-size",
    value: "32" + "\u00D7" + "32"
  },
  {
    selector: ".boxmodel-size > .boxmodel-width",
    value: "32"
  },
  {
    selector: ".boxmodel-size > .boxmodel-height",
    value: "32"
  },
  {
    selector: ".boxmodel-margin.boxmodel-top > span",
    value: 0
  },
  {
    selector: ".boxmodel-margin.boxmodel-left > span",
    value: 0
  },
  {
    selector: ".boxmodel-margin.boxmodel-bottom > span",
    value: 6
  },
  {
    selector: ".boxmodel-margin.boxmodel-right > span",
    value: 0
  },
  {
    selector: ".boxmodel-padding.boxmodel-top > span",
    value: 0
  },
  {
    selector: ".boxmodel-padding.boxmodel-left > span",
    value: 0
  },
  {
    selector: ".boxmodel-padding.boxmodel-bottom > span",
    value: 0
  },
  {
    selector: ".boxmodel-padding.boxmodel-right > span",
    value: 0
  },
  {
    selector: ".boxmodel-border.boxmodel-top > span",
    value: 0
  },
  {
    selector: ".boxmodel-border.boxmodel-left > span",
    value: 0
  },
  {
    selector: ".boxmodel-border.boxmodel-bottom > span",
    value: 0
  },
  {
    selector: ".boxmodel-border.boxmodel-right > span",
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
  yield testInitialValues(inspector, view);
});

function* testInitialValues(inspector, view) {
  info("Test that the initial values of the box model are correct");
  let viewdoc = view.document;

  for (let i = 0; i < res1.length; i++) {
    let elt = viewdoc.querySelector(res1[i].selector);
    is(elt.textContent, res1[i].value,
       res1[i].selector + " has the right value.");
  }
}
