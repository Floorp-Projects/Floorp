/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the box model displays the right values for the offset parent and that it
// updates when the node's style is changed

const TEST_URI = `
  <div id="relative_parent" style="position: relative">
    <div id="absolute_child" style="position: absolute"></div>
  </div>
  <div id="static"></div>
  <div id="no_parent" style="position: absolute"></div>
  <div id="fixed" style="position: fixed"></div>
`;

const OFFSET_PARENT_SELECTOR = ".computed-property-value-container .objectBox-node";

const res1 = [
  {
    selector: "#absolute_child",
    offsetParentValue: "div#relative_parent"
  },
  {
    selector: "#no_parent",
    offsetParentValue: "body"
  },
  {
    selector: "#relative_parent",
    offsetParentValue: "body"
  },
  {
    selector: "#static",
    offsetParentValue: null
  },
  {
    selector: "#fixed",
    offsetParentValue: null
  },
];

const updates = [
  {
    selector: "#absolute_child",
    update: "position: static"
  },
];

const res2 = [
  {
    selector: "#absolute_child",
    offsetParentValue: null
  },
];

add_task(async function() {
  await addTab("data:text/html," + encodeURIComponent(TEST_URI));
  const { inspector, boxmodel, testActor } = await openLayoutView();

  await testInitialValues(inspector, boxmodel);
  await testChangingValues(inspector, boxmodel, testActor);
});

async function testInitialValues(inspector, boxmodel) {
  info("Test that the initial values of the box model offset parent are correct");
  const viewdoc = boxmodel.document;

  for (const { selector, offsetParentValue } of res1) {
    await selectNode(selector, inspector);

    const elt = viewdoc.querySelector(OFFSET_PARENT_SELECTOR);
    is(elt && elt.textContent, offsetParentValue, selector + " has the right value.");
  }
}

async function testChangingValues(inspector, boxmodel, testActor) {
  info("Test that changing the document updates the box model");
  const viewdoc = boxmodel.document;

  for (const { selector, update } of updates) {
    const onUpdated = waitForUpdate(inspector);
    await testActor.setAttribute(selector, "style", update);
    await onUpdated;
  }

  for (const { selector, offsetParentValue } of res2) {
    await selectNode(selector, inspector);

    const elt = viewdoc.querySelector(OFFSET_PARENT_SELECTOR);
    is(elt && elt.textContent, offsetParentValue,
      selector + " has the right value after style update.");
  }
}
