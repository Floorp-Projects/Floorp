/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that if a tooltip is visible when a new selection is made, it closes

const TEST_URI = "<div class='one'>el 1</div><div class='two'>el 2</div>";
const XHTML_NS = "http://www.w3.org/1999/xhtml";

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode(".one", inspector);

  info("Testing rule view tooltip closes on new selection");
  yield testRuleView(view, inspector);

  info("Testing computed view tooltip closes on new selection");
  view = selectComputedView(inspector);
  yield testComputedView(view, inspector);
});

function* testRuleView(ruleView, inspector) {
  info("Showing the tooltip");

  let tooltip = ruleView.tooltips.previewTooltip;
  let tooltipContent = ruleView.styleDocument.createElementNS(XHTML_NS, "div");
  yield tooltip.setContent(tooltipContent, 100, 30);

  // Stop listening for mouse movements because it's not needed for this test,
  // and causes intermittent failures on Linux. When this test runs in the suite
  // sometimes a mouseleave event is dispatched at the start, which causes the
  // tooltip to hide in the middle of being shown, which causes timeouts later.
  tooltip.stopTogglingOnHover();

  let onShown = tooltip.once("shown");
  tooltip.show(ruleView.styleDocument.firstElementChild);
  yield onShown;

  info("Selecting a new node");
  let onHidden = tooltip.once("hidden");
  yield selectNode(".two", inspector);
  yield onHidden;

  ok(true, "Rule view tooltip closed after a new node got selected");
}

function* testComputedView(computedView, inspector) {
  info("Showing the tooltip");

  let tooltip = computedView.tooltips.previewTooltip;
  let tooltipContent = computedView.styleDocument.createElementNS(XHTML_NS, "div");
  yield tooltip.setContent(tooltipContent, 100, 30);
  // Stop listening for mouse movements because it's not needed for this test,
  // and causes intermittent failures on Linux. When this test runs in the suite
  // sometimes a mouseleave event is dispatched at the start, which causes the
  // tooltip to hide in the middle of being shown, which causes timeouts later.
  tooltip.stopTogglingOnHover();

  let onShown = tooltip.once("shown");
  tooltip.show(computedView.styleDocument.firstElementChild);
  yield onShown;

  info("Selecting a new node");
  let onHidden = tooltip.once("hidden");
  yield selectNode(".one", inspector);
  yield onHidden;

  ok(true, "Computed view tooltip closed after a new node got selected");
}
