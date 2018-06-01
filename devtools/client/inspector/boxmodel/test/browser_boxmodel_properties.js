/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the box model properties list displays the right values
// and that it updates when the node's style is changed.

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
  </style>
  <div>Test Node</div>
`;

const res1 = [
  {
    property: "box-sizing",
    value: "border-box"
  },
  {
    property: "display",
    value: "block"
  },
  {
    property: "float",
    value: "left"
  },
  {
    property: "line-height",
    value: "20px"
  },
  {
    property: "position",
    value: "relative"
  },
  {
    property: "z-index",
    value: 2
  },
];

const res2 = [
  {
    property: "box-sizing",
    value: "content-box"
  },
  {
    property: "display",
    value: "block"
  },
  {
    property: "float",
    value: "right"
  },
  {
    property: "line-height",
    value: "10px"
  },
  {
    property: "position",
    value: "static"
  },
  {
    property: "z-index",
    value: 5
  },
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, boxmodel, testActor } = await openLayoutView();
  await selectNode("div", inspector);

  await testInitialValues(inspector, boxmodel);
  await testChangingValues(inspector, boxmodel, testActor);
});

function testInitialValues(inspector, boxmodel) {
  info("Test that the initial values of the box model are correct");
  const doc = boxmodel.document;

  for (const { property, value } of res1) {
    const elt = doc.querySelector(getPropertySelector(property));
    is(elt.textContent, value, property + " has the right value.");
  }
}

async function testChangingValues(inspector, boxmodel, testActor) {
  info("Test that changing the document updates the box model");
  const doc = boxmodel.document;

  const onUpdated = waitForUpdate(inspector);
  await testActor.setAttribute("div", "style",
                               "box-sizing:content-box;float:right;" +
                               "line-height:10px;position:static;z-index:5;");
  await onUpdated;

  for (const { property, value } of res2) {
    const elt = doc.querySelector(getPropertySelector(property));
    is(elt.textContent, value, property + " has the right value after style update.");
  }
}

function getPropertySelector(propertyName) {
  return `.boxmodel-properties-wrapper .computed-property-view` +
  `[data-property-name=${propertyName}] .computed-property-value`;
}
