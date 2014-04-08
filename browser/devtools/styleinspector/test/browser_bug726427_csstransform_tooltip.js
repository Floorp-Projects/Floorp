/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the css transform preview tooltip is shown on transform properties

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  #testElement {',
  '    width: 500px;',
  '    height: 300px;',
  '    background: red;',
  '    transform: skew(16deg);',
  '  }',
  '  .test-element {',
  '    transform-origin: top left;',
  '    transform: rotate(45deg);',
  '  }',
  '  div {',
  '    transform: scaleX(1.5);',
  '    transform-origin: bottom right;',
  '  }',
  '  [attr] {',
  '  }',
  '</style>',
  '<div id="testElement" class="test-element" attr="value">transformed element</div>'
].join("\n");

let test = asyncTest(function*() {
  yield addTab("data:text/html,rule view css transform tooltip test");
  content.document.body.innerHTML = PAGE_CONTENT;
  let {toolbox, inspector, view} = yield openRuleView();

  info("Selecting the test node");
  yield selectNode("#testElement", inspector);

  info("Checking that transforms tooltips are shown in various rule-view properties");
  for (let selector of [".test-element", "div", "#testElement"]) {
    yield testTransformTooltipOnSelector(view, selector);
  }

  info("Checking that the transform tooltip doesn't appear for invalid transforms");
  yield testTransformTooltipNotShownOnInvalidTransform(view);

  info("Checking transforms in the computed-view");
  let {view} = yield openComputedView();
  yield testTransformTooltipOnComputedView(view);
});

function* testTransformTooltipOnSelector(view, selector) {
  info("Testing that a transform tooltip appears on transform in " + selector);

  let {valueSpan} = getRuleViewProperty(view, selector, "transform");
  ok(valueSpan, "The transform property was found");
  yield assertHoverTooltipOn(view.previewTooltip, valueSpan);

  // The transform preview is canvas, so there's not much we can test, so for
  // now, let's just be happy with the fact that the tooltips is shown!
  ok(true, "Tooltip shown on the transform property in " + selector);
}

function* testTransformTooltipNotShownOnInvalidTransform(view) {
  let ruleEditor;
  for (let rule of view._elementStyle.rules) {
    if (rule.matchedSelectors[0] === "[attr]") {
      ruleEditor = rule.editor;
    }
  }
  ruleEditor.addProperty("transform", "muchTransform(suchAngle)", "");

  let {valueSpan} = getRuleViewProperty(view, "[attr]", "transform");
  let isValid = yield isHoverTooltipTarget(view.previewTooltip, valueSpan);
  ok(!isValid, "The tooltip did not appear on hover of an invalid transform value");
}

function* testTransformTooltipOnComputedView(view) {
  info("Testing that a transform tooltip appears in the computed view too");

  let {valueSpan} = getComputedViewProperty(view, "transform");
  yield assertHoverTooltipOn(view.tooltip, valueSpan);

  // The transform preview is canvas, so there's not much we can test, so for
  // now, let's just be happy with the fact that the tooltips is shown!
  ok(true, "Tooltip shown on the computed transform property");
}
