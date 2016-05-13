/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the fontfamily tooltip on shorthand properties

const TEST_URI = `
  <style type="text/css">
    #testElement {
      font: italic bold .8em/1.2 Arial;
    }
  </style>
  <div id="testElement">test element</div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();

  yield selectNode("#testElement", inspector);
  yield testRuleView(view, inspector.selection.nodeFront);
});

function* testRuleView(ruleView, nodeFront) {
  info("Testing font-family tooltips in the rule view");

  let tooltip = ruleView.tooltips.previewTooltip;
  let panel = tooltip.panel;

  // Check that the rule view has a tooltip and that a XUL panel has
  // been created
  ok(tooltip, "Tooltip instance exists");
  ok(panel, "XUL panel exists");

  // Get the computed font family property inside the font rule view
  let propertyList = ruleView.element
    .querySelectorAll(".ruleview-propertylist");
  let fontExpander = propertyList[1].querySelectorAll(".ruleview-expander")[0];
  fontExpander.click();

  let rule = getRuleViewRule(ruleView, "#testElement");
  let valueSpan = rule
    .querySelector(".ruleview-computed .ruleview-propertyvalue");

  // And verify that the tooltip gets shown on this property
  yield assertHoverTooltipOn(tooltip, valueSpan);

  let images = panel.getElementsByTagName("image");
  is(images.length, 1, "Tooltip contains an image");
  ok(images[0].getAttribute("src")
    .startsWith("data:"), "Tooltip contains a data-uri image as expected");

  let dataURL = yield getFontFamilyDataURL(valueSpan.textContent, nodeFront);
  is(images[0].getAttribute("src"), dataURL,
    "Tooltip contains the correct data-uri image");
}
