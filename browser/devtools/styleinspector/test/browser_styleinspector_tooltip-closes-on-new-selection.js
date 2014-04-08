/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that if a tooltip is visible when a new selection is made, it closes

let test = asyncTest(function*() {
  yield addTab("data:text/html,<div class='one'>el 1</div><div class='two'>el 2</div>");

  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode(".one", inspector);

  info("Testing rule view tooltip closes on new selection");
  yield testRuleView(view, inspector);

  info("Testing computed view tooltip closes on new selection");
  let {view} = yield openComputedView();
  yield testComputedView(view, inspector);
});

function* testRuleView(ruleView, inspector) {
  info("Showing the tooltip");
  let tooltip = ruleView.previewTooltip;
  let onShown = tooltip.once("shown");
  tooltip.show();
  yield onShown;

  info("Selecting a new node");
  let onHidden = tooltip.once("hidden");
  yield selectNode(".two", inspector);

  ok(true, "Rule view tooltip closed after a new node got selected");
}

function* testComputedView(computedView, inspector) {
  info("Showing the tooltip");
  let tooltip = computedView.tooltip;
  let onShown = tooltip.once("shown");
  tooltip.show();
  yield onShown;

  info("Selecting a new node");
  let onHidden = tooltip.once("hidden");
  yield selectNode(".one", inspector);

  ok(true, "Computed view tooltip closed after a new node got selected");
}
