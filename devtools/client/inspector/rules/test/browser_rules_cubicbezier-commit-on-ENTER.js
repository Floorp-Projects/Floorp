/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a curve change in the cubic-bezier tooltip is committed when ENTER
// is pressed.

const TEST_URI = `
  <style type="text/css">
    body {
      transition: top 2s linear;
    }
  </style>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {view} = await openRuleView();

  info("Getting the bezier swatch element");
  const swatch = getRuleViewProperty(view, "body", "transition").valueSpan
    .querySelector(".ruleview-bezierswatch");

  await testPressingEnterCommitsChanges(swatch, view);
});

async function testPressingEnterCommitsChanges(swatch, ruleView) {
  const bezierTooltip = ruleView.tooltips.getTooltip("cubicBezier");

  info("Showing the tooltip");
  const onBezierWidgetReady = bezierTooltip.once("ready");
  swatch.click();
  await onBezierWidgetReady;

  const widget = await bezierTooltip.widget;
  info("Simulating a change of curve in the widget");
  widget.coordinates = [0.1, 2, 0.9, -1];
  const expected = "cubic-bezier(0.1, 2, 0.9, -1)";

  await waitForSuccess(async function() {
    const func = await getComputedStyleProperty("body", null,
                                              "transition-timing-function");
    return func === expected;
  }, "Waiting for the change to be previewed on the element");

  ok(getRuleViewProperty(ruleView, "body", "transition").valueSpan.textContent
    .includes("cubic-bezier("),
    "The text of the timing-function was updated");

  info("Sending RETURN key within the tooltip document");
  // Pressing RETURN ends up doing 2 rule-view updates, one for the preview and
  // one for the commit when the tooltip closes.
  const onRuleViewChanged = waitForNEvents(ruleView, "ruleview-changed", 2);
  focusAndSendKey(widget.parent.ownerDocument.defaultView, "RETURN");
  await onRuleViewChanged;

  const style = await getComputedStyleProperty("body", null,
                                             "transition-timing-function");
  is(style, expected, "The element's timing-function was kept after RETURN");

  const ruleViewStyle = getRuleViewProperty(ruleView, "body", "transition")
                      .valueSpan.textContent.includes("cubic-bezier(");
  ok(ruleViewStyle, "The text of the timing-function was kept after RETURN");
}
