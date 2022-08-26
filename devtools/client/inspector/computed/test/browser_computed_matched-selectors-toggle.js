/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the computed view properties can be expanded and collapsed with
// either the twisty or by dbl-clicking on the container.

const TEST_URI = `
  <style type="text/css"> ,
    html { color: #000000; font-size: 15pt; }
    h1 { color: red; }
  </style>
  <h1>Some header text</h1>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openComputedView();
  await selectNode("h1", inspector);

  await testExpandOnTwistyClick(view, inspector);
  await testCollapseOnTwistyClick(view, inspector);
  await testExpandOnDblClick(view, inspector);
  await testCollapseOnDblClick(view, inspector);
});

async function testExpandOnTwistyClick(
  { styleDocument, styleWindow },
  inspector
) {
  info("Testing that a property expands on twisty click");

  info("Getting twisty element");
  const twisty = styleDocument.querySelector(".computed-expandable");
  ok(twisty, "Twisty found");

  const onExpand = inspector.once("computed-view-property-expanded");
  info("Clicking on the twisty element");
  twisty.click();

  await onExpand;

  // Expanded means the matchedselectors div is not empty
  const div = styleDocument.querySelector(
    ".computed-property-content .matchedselectors"
  );
  ok(!!div.childNodes.length, "Matched selectors are expanded on twisty click");
}

async function testCollapseOnTwistyClick(
  { styleDocument, styleWindow },
  inspector
) {
  info("Testing that a property collapses on twisty click");

  info("Getting twisty element");
  const twisty = styleDocument.querySelector(".computed-expandable");
  ok(twisty, "Twisty found");

  const onCollapse = inspector.once("computed-view-property-collapsed");
  info("Clicking on the twisty element");
  twisty.click();

  await onCollapse;

  // Collapsed means the matchedselectors div is empty
  const div = styleDocument.querySelector(
    ".computed-property-content .matchedselectors"
  );
  ok(
    div.childNodes.length === 0,
    "Matched selectors are collapsed on twisty click"
  );
}

async function testExpandOnDblClick({ styleDocument, styleWindow }, inspector) {
  info("Testing that a property expands on container dbl-click");

  info("Getting computed property container");
  const container = styleDocument.querySelector(
    "#computed-container .computed-property-view"
  );
  ok(container, "Container found");

  container.scrollIntoView();

  const onExpand = inspector.once("computed-view-property-expanded");
  info("Dbl-clicking on the container");
  EventUtils.synthesizeMouseAtCenter(container, { clickCount: 2 }, styleWindow);

  await onExpand;

  // Expanded means the matchedselectors div is not empty
  const div = styleDocument.querySelector(
    ".computed-property-content .matchedselectors"
  );
  ok(!!div.childNodes.length, "Matched selectors are expanded on dblclick");
}

async function testCollapseOnDblClick(
  { styleDocument, styleWindow },
  inspector
) {
  info("Testing that a property collapses on container dbl-click");

  info("Getting computed property container");
  const container = styleDocument.querySelector(
    "#computed-container .computed-property-view"
  );
  ok(container, "Container found");

  const onCollapse = inspector.once("computed-view-property-collapsed");
  info("Dbl-clicking on the container");
  EventUtils.synthesizeMouseAtCenter(container, { clickCount: 2 }, styleWindow);

  await onCollapse;

  // Collapsed means the matchedselectors div is empty
  const div = styleDocument.querySelector(
    ".computed-property-content .matchedselectors"
  );
  ok(
    div.childNodes.length === 0,
    "Matched selectors are collapsed on dblclick"
  );
}
