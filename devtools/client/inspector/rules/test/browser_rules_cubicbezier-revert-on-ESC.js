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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {view} = yield openRuleView();
  yield testPressingEscapeRevertsChanges(view);
  yield testPressingEscapeRevertsChangesAndDisables(view);
});

function* testPressingEscapeRevertsChanges(view) {
  let {propEditor} = yield openCubicBezierAndChangeCoords(view, 1, 0,
    [0.1, 2, 0.9, -1], {
      selector: "body",
      name: "animation-timing-function",
      value: "cubic-bezier(0.1, 2, 0.9, -1)"
    });

  is(propEditor.valueSpan.textContent, "cubic-bezier(.1,2,.9,-1)",
    "Got expected property value.");

  yield escapeTooltip(view);

  yield waitForComputedStyleProperty("body", null, "animation-timing-function",
    "linear");
  is(propEditor.valueSpan.textContent, "linear",
    "Got expected property value.");
}

function* testPressingEscapeRevertsChangesAndDisables(view) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let textProp = ruleEditor.rule.textProps[0];
  let propEditor = textProp.editor;

  info("Disabling animation-timing-function property");
  yield togglePropStatus(view, textProp);

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

  yield openCubicBezierAndChangeCoords(view, 1, 0, [0.1, 2, 0.9, -1]);

  yield escapeTooltip(view);

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

function* escapeTooltip(view) {
  info("Pressing ESCAPE to close the tooltip");

  let bezierTooltip = view.tooltips.cubicBezier;
  let widget = yield bezierTooltip.widget;
  let onHidden = bezierTooltip.tooltip.once("hidden");
  let onModifications = view.once("ruleview-changed");
  focusAndSendKey(widget.parent.ownerDocument.defaultView, "ESCAPE");
  yield onHidden;
  yield onModifications;
}
