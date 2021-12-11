/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const L10N = new LocalizationHelper(
  "devtools/client/locales/inspector.properties"
);

// Test that hovering over a box model value with no associated rule will show a tooltip
// saying: "No associated rule".

const TEST_URI = `<style>
    #box {}
  </style>
  <div id="box"></div>`;

add_task(async function() {
  await pushPref("devtools.layout.boxmodel.highlightProperty", true);
  await addTab("data:text/html," + encodeURIComponent(TEST_URI));
  const { inspector, boxmodel } = await openLayoutView();
  const { rulePreviewTooltip } = boxmodel;
  await selectNode("#box", inspector);

  info(
    "Test that hovering over margin-top shows tooltip showing 'No associated rule'."
  );
  const el = boxmodel.document.querySelector(
    ".boxmodel-margin.boxmodel-top > span"
  );

  info("Wait for mouse to hover over margin-top element.");
  const onRulePreviewTooltipShown = rulePreviewTooltip._tooltip.once(
    "shown",
    () => {
      ok(true, "Tooltip shown.");
      is(
        rulePreviewTooltip.message.textContent,
        L10N.getStr("rulePreviewTooltip.noAssociatedRule"),
        `Tooltip shows ${L10N.getStr("rulePreviewTooltip.noAssociatedRule")}`
      );
    }
  );
  EventUtils.synthesizeMouseAtCenter(
    el,
    { type: "mousemove", shiftKey: true },
    boxmodel.document.defaultView
  );
  await onRulePreviewTooltipShown;
});
