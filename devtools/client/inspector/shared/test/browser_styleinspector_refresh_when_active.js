/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule and computed view refreshes when they are active.

const TEST_URI = `
  <div id="one" style="color:red;">one</div>
  <div id="two" style="color:blue;">two</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();

  await selectNode("#one", inspector);

  is(getRuleViewPropertyValue(view, "element", "color"), "red",
    "The rule-view shows the properties for test node one");

  info("Switching to the computed-view");
  const onComputedViewReady = inspector.once("computed-view-refreshed");
  selectComputedView(inspector);
  await onComputedViewReady;
  const cView = inspector.getPanel("computedview").computedView;

  is(getComputedViewPropertyValue(cView, "color"), "rgb(255, 0, 0)",
    "The computed-view shows the properties for test node one");

  info("Selecting test node two");
  await selectNode("#two", inspector);

  is(getComputedViewPropertyValue(cView, "color"), "rgb(0, 0, 255)",
    "The computed-view shows the properties for test node two");
  is(getRuleViewPropertyValue(view, "element", "color"), "blue",
    "The rule-view shows the properties for test node two");
});
