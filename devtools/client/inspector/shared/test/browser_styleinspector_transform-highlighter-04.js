/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the css transform highlighter is shown only when hovering over a
// transform declaration that isn't overriden or disabled

// Note that unlike the other browser_styleinspector_transform-highlighter-N.js
// tests, this one only tests the rule-view as only this view features disabled
// and overriden properties

const TEST_URI = `
  <style type="text/css">
    div {
      background: purple;
      width:300px;height:300px;
      transform: rotate(16deg);
    }
    .test {
      transform: skew(25deg);
    }
  </style>
  <div class="test"></div>
`;

const TYPE = "CssTransformHighlighter";

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode(".test", inspector);

  let hs = view.highlighters;

  info("Faking a mousemove on the overriden property");
  let {valueSpan} = getRuleViewProperty(view, "div", "transform");
  hs.onMouseMove({target: valueSpan});
  ok(!hs.highlighters[TYPE],
    "No highlighter was created for the overriden property");

  info("Disabling the applied property");
  let classRuleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = classRuleEditor.rule.textProps[0].editor;
  propEditor.enable.click();
  yield classRuleEditor.rule._applyingModifications;

  info("Faking a mousemove on the disabled property");
  ({valueSpan} = getRuleViewProperty(view, ".test", "transform"));
  hs.onMouseMove({target: valueSpan});
  ok(!hs.highlighters[TYPE],
    "No highlighter was created for the disabled property");

  info("Faking a mousemove on the now unoverriden property");
  ({valueSpan} = getRuleViewProperty(view, "div", "transform"));
  let onHighlighterShown = hs.once("highlighter-shown");
  hs.onMouseMove({target: valueSpan});
  yield onHighlighterShown;
});
