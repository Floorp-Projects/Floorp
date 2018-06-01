/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that changes made to the cubic-bezier timing-function in the
// cubic-bezier tooltip are reverted when ESC is pressed.

const TEST_URI = `
  <style type='text/css'>
    body {
      animation-timing-function: linear;
    }
  </style>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {view} = await openRuleView();
  await testPressingEscapeRevertsChanges(view);
});

async function testPressingEscapeRevertsChanges(view) {
  const {propEditor} = await openCubicBezierAndChangeCoords(view, 1, 0,
    [0.1, 2, 0.9, -1], {
      selector: "body",
      name: "animation-timing-function",
      value: "cubic-bezier(0.1, 2, 0.9, -1)"
    });

  is(propEditor.valueSpan.textContent, "cubic-bezier(.1,2,.9,-1)",
    "Got expected property value.");

  await escapeTooltip(view);

  await waitForComputedStyleProperty("body", null, "animation-timing-function",
    "linear");
  is(propEditor.valueSpan.textContent, "linear",
    "Got expected property value.");
}

async function escapeTooltip(view) {
  info("Pressing ESCAPE to close the tooltip");

  const bezierTooltip = view.tooltips.getTooltip("cubicBezier");
  const widget = await bezierTooltip.widget;
  const onHidden = bezierTooltip.tooltip.once("hidden");
  const onModifications = view.once("ruleview-changed");
  focusAndSendKey(widget.parent.ownerDocument.defaultView, "ESCAPE");
  await onHidden;
  await onModifications;
}
