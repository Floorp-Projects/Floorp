/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the fontfamily tooltip on longhand properties

const TEST_URI = `
  <style type="text/css">
    #testElement {
      font-family: cursive;
      color: #333;
      padding-left: 70px;
    }
  </style>
  <div id="testElement">test element</div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testElement", inspector);
  yield testRuleView(view, inspector.selection.nodeFront);

  info("Opening the computed view");
  let onComputedViewReady = inspector.once("computed-view-refreshed");
  view = selectComputedView(inspector);
  yield onComputedViewReady;

  yield testComputedView(view, inspector.selection.nodeFront);

  yield testExpandedComputedViewProperty(view, inspector.selection.nodeFront);
});

function* testRuleView(ruleView, nodeFront) {
  info("Testing font-family tooltips in the rule view");

  let tooltip = ruleView.tooltips.getTooltip("previewTooltip");
  let panel = tooltip.panel;

  // Check that the rule view has a tooltip and that a XUL panel has
  // been created
  ok(tooltip, "Tooltip instance exists");
  ok(panel, "XUL panel exists");

  // Get the font family property inside the rule view
  let {valueSpan} = getRuleViewProperty(ruleView, "#testElement",
    "font-family");

  // And verify that the tooltip gets shown on this property
  valueSpan.scrollIntoView(true);
  let previewTooltip = yield assertShowPreviewTooltip(ruleView, valueSpan);

  let images = panel.getElementsByTagName("img");
  is(images.length, 1, "Tooltip contains an image");
  ok(images[0].getAttribute("src").startsWith("data:"),
    "Tooltip contains a data-uri image as expected");

  let dataURL = yield getFontFamilyDataURL(valueSpan.textContent, nodeFront);
  is(images[0].getAttribute("src"), dataURL,
    "Tooltip contains the correct data-uri image");

  yield assertTooltipHiddenOnMouseOut(previewTooltip, valueSpan);
}

function* testComputedView(computedView, nodeFront) {
  info("Testing font-family tooltips in the computed view");

  let tooltip = computedView.tooltips.getTooltip("previewTooltip");
  let panel = tooltip.panel;
  let {valueSpan} = getComputedViewProperty(computedView, "font-family");

  valueSpan.scrollIntoView(true);
  let previewTooltip = yield assertShowPreviewTooltip(computedView, valueSpan);

  let images = panel.getElementsByTagName("img");
  is(images.length, 1, "Tooltip contains an image");
  ok(images[0].getAttribute("src").startsWith("data:"),
    "Tooltip contains a data-uri image as expected");

  let dataURL = yield getFontFamilyDataURL(valueSpan.textContent, nodeFront);
  is(images[0].getAttribute("src"), dataURL,
    "Tooltip contains the correct data-uri image");

  yield assertTooltipHiddenOnMouseOut(previewTooltip, valueSpan);
}

function* testExpandedComputedViewProperty(computedView, nodeFront) {
  info("Testing font-family tooltips in expanded properties of the " +
    "computed view");

  info("Expanding the font-family property to reveal matched selectors");
  let propertyView = getPropertyView(computedView, "font-family");
  propertyView.matchedExpanded = true;
  yield propertyView.refreshMatchedSelectors();

  let valueSpan = propertyView.matchedSelectorsContainer
    .querySelector(".bestmatch .other-property-value");

  let tooltip = computedView.tooltips.getTooltip("previewTooltip");
  let panel = tooltip.panel;

  valueSpan.scrollIntoView(true);
  let previewTooltip = yield assertShowPreviewTooltip(computedView, valueSpan);

  let images = panel.getElementsByTagName("img");
  is(images.length, 1, "Tooltip contains an image");
  ok(images[0].getAttribute("src").startsWith("data:"),
    "Tooltip contains a data-uri image as expected");

  let dataURL = yield getFontFamilyDataURL(valueSpan.textContent, nodeFront);
  is(images[0].getAttribute("src"), dataURL,
    "Tooltip contains the correct data-uri image");

  yield assertTooltipHiddenOnMouseOut(previewTooltip, valueSpan);
}

function getPropertyView(computedView, name) {
  let propertyView = null;
  computedView.propertyViews.some(function (view) {
    if (view.name == name) {
      propertyView = view;
      return true;
    }
    return false;
  });
  return propertyView;
}
