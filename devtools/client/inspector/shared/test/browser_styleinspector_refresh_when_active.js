/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the style-inspector views only refresh when they are active.

const TEST_URI = `
  <div id="one" style="color:red;">one</div>
  <div id="two" style="color:blue;">two</div>
`;

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#one", inspector);

  is(getRuleViewPropertyValue(view, "element", "color"), "red",
    "The rule-view shows the properties for test node one");

  let cView = inspector.computedview.view;
  let prop = getComputedViewProperty(cView, "color");
  ok(!prop, "The computed-view doesn't show the properties for test node one");

  info("Switching to the computed-view");
  let onComputedViewReady = inspector.once("computed-view-refreshed");
  yield openComputedView();
  yield onComputedViewReady;

  ok(getComputedViewPropertyValue(cView, "color"), "#F00",
    "The computed-view shows the properties for test node one");

  info("Selecting test node two");
  yield selectNode("#two", inspector);

  ok(getComputedViewPropertyValue(cView, "color"), "#00F",
    "The computed-view shows the properties for test node two");

  is(getRuleViewPropertyValue(view, "element", "color"), "red",
    "The rule-view doesn't the properties for test node two");
});
