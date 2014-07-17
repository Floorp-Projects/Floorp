/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the computed view properties can be expanded and collapsed with
// either the twisty or by dbl-clicking on the container

const TEST_URL = "data:text/html;charset=utf-8," + encodeURIComponent([
  '<html>' +
  '<head>' +
  '  <title>Computed view toggling test</title>',
  '  <style type="text/css"> ',
  '    html { color: #000000; font-size: 15pt; } ',
  '    h1 { color: red; } ',
  '  </style>',
  '</head>',
  '<body>',
  '  <h1>Some header text</h1>',
  '</body>',
  '</html>'
].join("\n"));

let test = asyncTest(function*() {
  yield addTab(TEST_URL);
  let {toolbox, inspector, view} = yield openComputedView();

  info("Selecting the test node");
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
  ok(div.childNodes.length > 0, "Matched selectors are expanded on twisty click");
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
  ok(div.childNodes.length === 0, "Matched selectors are collapsed on twisty click");
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
  ok(div.childNodes.length === 0, "Matched selectors are collapsed on dblclick");
}
