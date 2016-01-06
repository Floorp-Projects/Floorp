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

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {view} = yield openRuleView();
  yield testPressingEscapeRevertsChanges(view);
  yield testPressingEscapeRevertsChangesAndDisables(view);
});

function* testPressingEscapeRevertsChanges(view) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[0].editor;
  let swatch = propEditor.valueSpan.querySelector(".ruleview-bezierswatch");
  let bezierTooltip = view.tooltips.cubicBezier;

  let onShown = bezierTooltip.tooltip.once("shown");
  swatch.click();
  yield onShown;

  let widget = yield bezierTooltip.widget;
  info("Simulating a change of curve in the widget");
  widget.coordinates = [0.1, 2, 0.9, -1];
  yield ruleEditor.rule._applyingModifications;

  yield waitForComputedStyleProperty("body", null, "animation-timing-function",
    "cubic-bezier(0.1, 2, 0.9, -1)");
  is(propEditor.valueSpan.textContent, "cubic-bezier(.1,2,.9,-1)",
    "Got expected property value.");

  info("Pressing ESCAPE to close the tooltip");
  let onHidden = bezierTooltip.tooltip.once("hidden");
  EventUtils.sendKey("ESCAPE", widget.parent.ownerDocument.defaultView);
  yield onHidden;
  yield ruleEditor.rule._applyingModifications;

  yield waitForComputedStyleProperty("body", null, "animation-timing-function",
    "linear");
  is(propEditor.valueSpan.textContent, "linear",
    "Got expected property value.");
}

function* testPressingEscapeRevertsChangesAndDisables(view) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[0].editor;
  let swatch = propEditor.valueSpan.querySelector(".ruleview-bezierswatch");
  let bezierTooltip = view.tooltips.cubicBezier;

  info("Disabling animation-timing-function property");
  propEditor.enable.click();
  yield ruleEditor.rule._applyingModifications;

  ok(propEditor.element.classList.contains("ruleview-overridden"),
    "property is overridden.");
  is(propEditor.enable.style.visibility, "visible",
    "property enable checkbox is visible.");
  ok(!propEditor.enable.getAttribute("checked"),
    "property enable checkbox is not checked.");
  ok(!propEditor.prop.enabled,
    "animation-timing-function property is disabled.");
  let newValue = yield getRulePropertyValue("animation-timing-function");
  is(newValue, "", "animation-timing-function should have been unset.");

  let onShown = bezierTooltip.tooltip.once("shown");
  swatch.click();
  yield onShown;

  ok(!propEditor.element.classList.contains("ruleview-overridden"),
    "property overridden is not displayed.");
  is(propEditor.enable.style.visibility, "hidden",
    "property enable checkbox is hidden.");

  let widget = yield bezierTooltip.widget;
  info("Simulating a change of curve in the widget");
  widget.coordinates = [0.1, 2, 0.9, -1];
  yield ruleEditor.rule._applyingModifications;

  info("Pressing ESCAPE to close the tooltip");
  let onHidden = bezierTooltip.tooltip.once("hidden");
  EventUtils.sendKey("ESCAPE", widget.parent.ownerDocument.defaultView);
  yield onHidden;
  yield ruleEditor.rule._applyingModifications;

  ok(propEditor.element.classList.contains("ruleview-overridden"),
    "property is overridden.");
  is(propEditor.enable.style.visibility, "visible",
    "property enable checkbox is visible.");
  ok(!propEditor.enable.getAttribute("checked"),
    "property enable checkbox is not checked.");
  ok(!propEditor.prop.enabled,
    "animation-timing-function property is disabled.");
  newValue = yield getRulePropertyValue("animation-timing-function");
  is(newValue, "", "animation-timing-function should have been unset.");
  is(propEditor.valueSpan.textContent, "linear",
    "Got expected property value.");
}

function* getRulePropertyValue(name) {
  let propValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: name
  });
  return propValue;
}
