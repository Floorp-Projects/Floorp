/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that hovering over a box model value shows tooltip with the source CSS rule
// if there is an associated rule.

const TEST_URI = `<style>
      #box { margin-left: 1px; padding: 2px 3px; border: solid 4em; }
    </style>
    <div id="box"></div>`;

add_task(async function() {
  await pushPref("devtools.layout.boxmodel.highlightProperty", true);
  await addTab("data:text/html," + encodeURIComponent(TEST_URI));
  const { inspector, boxmodel } = await openLayoutView();
  await selectNode("#box", inspector);

  // Test value with associated rule for margin
  info(
    "Test that hovering over margin-left shows tooltip containing the CSS declaration for margin."
  );
  await testBoxmodelTooltip(
    boxmodel,
    ".boxmodel-margin.boxmodel-left > span",
    "margin-left: 1px"
  );

  // Test value with associated rule for padding
  info(
    "Test that hovering over padding-left shows tooltip containing the CSS declaration for padding."
  );
  await testBoxmodelTooltip(
    boxmodel,
    ".boxmodel-padding.boxmodel-left > span",
    "padding: 2px 3px"
  );

  // Test value with associated rule for border
  info(
    "Test that hovering over border-left shows tooltip containing the CSS declaration for border."
  );
  await testBoxmodelTooltip(
    boxmodel,
    ".boxmodel-border.boxmodel-left > span",
    "border: solid 4em"
  );
});

async function testBoxmodelTooltip(boxmodel, valueSelector, expectedTooltip) {
  // const { rulePreviewTooltip } = boxmodel;
  const el = boxmodel.document.querySelector(valueSelector);
  info("Wait for mouse to hover over value element");
  const onRulePreviewTooltipShown = boxmodel.rulePreviewTooltip._tooltip.once(
    "shown",
    () => {
      info("Check that tooltip is shown.");
      is(
        boxmodel.rulePreviewTooltip.message.textContent.includes(
          expectedTooltip
        ),
        true,
        `Tooltip shows '${expectedTooltip}'`
      );
    }
  );
  EventUtils.synthesizeMouseAtCenter(
    el,
    { type: "mousemove" },
    boxmodel.document.defaultView
  );
  await onRulePreviewTooltipShown;
}
