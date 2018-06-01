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

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = await openRuleView();
  await selectNode("#testElement", inspector);
  await testRuleView(view, inspector.selection.nodeFront);

  info("Opening the computed view");
  const onComputedViewReady = inspector.once("computed-view-refreshed");
  view = selectComputedView(inspector);
  await onComputedViewReady;

  await testComputedView(view, inspector.selection.nodeFront);

  await testExpandedComputedViewProperty(view, inspector.selection.nodeFront);
});

async function testRuleView(ruleView, nodeFront) {
  info("Testing font-family tooltips in the rule view");

  const tooltip = ruleView.tooltips.getTooltip("previewTooltip");
  const panel = tooltip.panel;

  // Check that the rule view has a tooltip and that a XUL panel has
  // been created
  ok(tooltip, "Tooltip instance exists");
  ok(panel, "XUL panel exists");

  // Get the font family property inside the rule view
  const {valueSpan} = getRuleViewProperty(ruleView, "#testElement",
    "font-family");

  // And verify that the tooltip gets shown on this property
  valueSpan.scrollIntoView(true);
  let previewTooltip = await assertShowPreviewTooltip(ruleView, valueSpan);

  let images = panel.getElementsByTagName("img");
  is(images.length, 1, "Tooltip contains an image");
  ok(images[0].getAttribute("src").startsWith("data:"),
    "Tooltip contains a data-uri image as expected");

  let dataURL = await getFontFamilyDataURL(valueSpan.textContent, nodeFront);
  is(images[0].getAttribute("src"), dataURL,
    "Tooltip contains the correct data-uri image");

  await assertTooltipHiddenOnMouseOut(previewTooltip, valueSpan);

  // Do the tooltip test again, but now when hovering on the span that
  // encloses each and every font family.
  const fontFamilySpan = valueSpan.querySelector(".ruleview-font-family");
  fontFamilySpan.scrollIntoView(true);

  previewTooltip = await assertShowPreviewTooltip(ruleView, fontFamilySpan);

  images = panel.getElementsByTagName("img");
  is(images.length, 1, "Tooltip contains an image");
  ok(images[0].getAttribute("src").startsWith("data:"),
    "Tooltip contains a data-uri image as expected");

  dataURL = await getFontFamilyDataURL(fontFamilySpan.textContent, nodeFront);
  is(images[0].getAttribute("src"), dataURL,
    "Tooltip contains the correct data-uri image");

  await assertTooltipHiddenOnMouseOut(previewTooltip, fontFamilySpan);
}

async function testComputedView(computedView, nodeFront) {
  info("Testing font-family tooltips in the computed view");

  const tooltip = computedView.tooltips.getTooltip("previewTooltip");
  const panel = tooltip.panel;
  const {valueSpan} = getComputedViewProperty(computedView, "font-family");

  valueSpan.scrollIntoView(true);
  const previewTooltip = await assertShowPreviewTooltip(computedView, valueSpan);

  const images = panel.getElementsByTagName("img");
  is(images.length, 1, "Tooltip contains an image");
  ok(images[0].getAttribute("src").startsWith("data:"),
    "Tooltip contains a data-uri image as expected");

  const dataURL = await getFontFamilyDataURL(valueSpan.textContent, nodeFront);
  is(images[0].getAttribute("src"), dataURL,
    "Tooltip contains the correct data-uri image");

  await assertTooltipHiddenOnMouseOut(previewTooltip, valueSpan);
}

async function testExpandedComputedViewProperty(computedView, nodeFront) {
  info("Testing font-family tooltips in expanded properties of the " +
    "computed view");

  info("Expanding the font-family property to reveal matched selectors");
  const propertyView = getPropertyView(computedView, "font-family");
  propertyView.matchedExpanded = true;
  await propertyView.refreshMatchedSelectors();

  const valueSpan = propertyView.matchedSelectorsContainer
    .querySelector(".bestmatch .computed-other-property-value");

  const tooltip = computedView.tooltips.getTooltip("previewTooltip");
  const panel = tooltip.panel;

  valueSpan.scrollIntoView(true);
  const previewTooltip = await assertShowPreviewTooltip(computedView, valueSpan);

  const images = panel.getElementsByTagName("img");
  is(images.length, 1, "Tooltip contains an image");
  ok(images[0].getAttribute("src").startsWith("data:"),
    "Tooltip contains a data-uri image as expected");

  const dataURL = await getFontFamilyDataURL(valueSpan.textContent, nodeFront);
  is(images[0].getAttribute("src"), dataURL,
    "Tooltip contains the correct data-uri image");

  await assertTooltipHiddenOnMouseOut(previewTooltip, valueSpan);
}

function getPropertyView(computedView, name) {
  let propertyView = null;
  computedView.propertyViews.some(function(view) {
    if (view.name == name) {
      propertyView = view;
      return true;
    }
    return false;
  });
  return propertyView;
}
