/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that changes made to the cubic-bezier timing-function in the cubic-bezier
// tooltip are reverted when ESC is pressed

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    animation-timing-function: linear;',
  '  }',
  '</style>',
].join("\n");

let test = asyncTest(function*() {
  yield addTab("data:text/html;charset=utf-8,rule view cubic-bezier tooltip test");
  content.document.body.innerHTML = PAGE_CONTENT;
  let {toolbox, inspector, view} = yield openRuleView();

  info("Getting the bezier swatch element");
  let swatch = getRuleViewProperty(view, "body", "animation-timing-function").valueSpan
    .querySelector(".ruleview-bezierswatch");
  yield testPressingEscapeRevertsChanges(swatch, view);
});

function* testPressingEscapeRevertsChanges(swatch, ruleView) {
  let bezierTooltip = ruleView.tooltips.cubicBezier;

  let onShown = bezierTooltip.tooltip.once("shown");
  swatch.click();
  yield onShown;

  let widget = yield bezierTooltip.widget;
  info("Simulating a change of curve in the widget");
  widget.coordinates = [0.1, 2, 0.9, -1];
  let expected = "cubic-bezier(0.1, 2, 0.9, -1)";

  yield waitForSuccess(() => {
    return content.getComputedStyle(content.document.body).animationTimingFunction === expected;
  }, "Waiting for the change to be previewed on the element");

  info("Pressing ESCAPE to close the tooltip");
  let onHidden = bezierTooltip.tooltip.once("hidden");
  EventUtils.sendKey("ESCAPE", widget.parent.ownerDocument.defaultView);
  yield onHidden;

  yield waitForSuccess(() => {
    return content.getComputedStyle(content.document.body).animationTimingFunction === "cubic-bezier(0, 0, 1, 1)";
  }, "Waiting for the change to be reverted on the element");
}
