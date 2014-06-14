/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the css transform highlighter is shown only when hovering over a
// transform declaration that isn't overriden or disabled

// Note that unlike the other browser_styleinspector_transform-highlighter-N.js
// tests, this one only tests the rule-view as only this view features disabled
// and overriden properties

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  div {',
  '    background: purple;',
  '    width:300px;height:300px;',
  '    transform: rotate(16deg);',
  '  }',
  '  .test {',
  '    transform: skew(25deg);',
  '  }',
  '</style>',
  '<div class="test"></div>'
].join("\n");

let test = asyncTest(function*() {
  yield addTab("data:text/html," + PAGE_CONTENT);

  let {view: rView, inspector} = yield openRuleView();
  yield selectNode(".test", inspector);

  info("Faking a mousemove on the overriden property");
  let {valueSpan} = getRuleViewProperty(rView, "div", "transform");
  rView._onMouseMove({target: valueSpan});
  ok(!rView.transformHighlighter, "No highlighter was created for the overriden property");
  ok(!rView.transformHighlighterPromise, "And no highlighter is being initialized either");

  info("Disabling the applied property");
  let classRuleEditor = rView.element.children[1]._ruleEditor;
  let propEditor = classRuleEditor.rule.textProps[0].editor;
  propEditor.enable.click();
  yield classRuleEditor.rule._applyingModifications;

  info("Faking a mousemove on the disabled property");
  let {valueSpan} = getRuleViewProperty(rView, ".test", "transform");
  rView._onMouseMove({target: valueSpan});
  ok(!rView.transformHighlighter, "No highlighter was created for the disabled property");
  ok(!rView.transformHighlighterPromise, "And no highlighter is being initialized either");

  info("Faking a mousemove on the now unoverriden property");
  let {valueSpan} = getRuleViewProperty(rView, "div", "transform");
  rView._onMouseMove({target: valueSpan});
  ok(rView.transformHighlighterPromise, "The highlighter is being initialized now");
  let h = yield rView.transformHighlighterPromise;
  is(h, rView.transformHighlighter, "The initialized highlighter is the right one");
});
