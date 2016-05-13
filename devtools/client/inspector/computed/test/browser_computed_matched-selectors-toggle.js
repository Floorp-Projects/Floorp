/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openComputedView();
  yield selectNode("h1", inspector);

  yield testExpandOnTwistyClick(view, inspector);
  yield testCollapseOnTwistyClick(view, inspector);
  yield testExpandOnDblClick(view, inspector);
  yield testCollapseOnDblClick(view, inspector);
});

function* testExpandOnTwistyClick({styleDocument, styleWindow}, inspector) {
  info("Testing that a property expands on twisty click");

  info("Getting twisty element");
  let twisty = styleDocument.querySelector(".expandable");
  ok(twisty, "Twisty found");

  let onExpand = inspector.once("computed-view-property-expanded");
  info("Clicking on the twisty element");
  twisty.click();

  yield onExpand;

  // Expanded means the matchedselectors div is not empty
  let div = styleDocument.querySelector(".property-content .matchedselectors");
  ok(div.childNodes.length > 0,
    "Matched selectors are expanded on twisty click");
}

function* testCollapseOnTwistyClick({styleDocument, styleWindow}, inspector) {
  info("Testing that a property collapses on twisty click");

  info("Getting twisty element");
  let twisty = styleDocument.querySelector(".expandable");
  ok(twisty, "Twisty found");

  let onCollapse = inspector.once("computed-view-property-collapsed");
  info("Clicking on the twisty element");
  twisty.click();

  yield onCollapse;

  // Collapsed means the matchedselectors div is empty
  let div = styleDocument.querySelector(".property-content .matchedselectors");
  ok(div.childNodes.length === 0,
    "Matched selectors are collapsed on twisty click");
}

function* testExpandOnDblClick({styleDocument, styleWindow}, inspector) {
  info("Testing that a property expands on container dbl-click");

  info("Getting computed property container");
  let container = styleDocument.querySelector(".property-view");
  ok(container, "Container found");

  let onExpand = inspector.once("computed-view-property-expanded");
  info("Dbl-clicking on the container");
  EventUtils.synthesizeMouseAtCenter(container, {clickCount: 2}, styleWindow);

  yield onExpand;

  // Expanded means the matchedselectors div is not empty
  let div = styleDocument.querySelector(".property-content .matchedselectors");
  ok(div.childNodes.length > 0, "Matched selectors are expanded on dblclick");
}

function* testCollapseOnDblClick({styleDocument, styleWindow}, inspector) {
  info("Testing that a property collapses on container dbl-click");

  info("Getting computed property container");
  let container = styleDocument.querySelector(".property-view");
  ok(container, "Container found");

  let onCollapse = inspector.once("computed-view-property-collapsed");
  info("Dbl-clicking on the container");
  EventUtils.synthesizeMouseAtCenter(container, {clickCount: 2}, styleWindow);

  yield onCollapse;

  // Collapsed means the matchedselectors div is empty
  let div = styleDocument.querySelector(".property-content .matchedselectors");
  ok(div.childNodes.length === 0,
    "Matched selectors are collapsed on dblclick");
}
