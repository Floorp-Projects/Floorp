/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the css transform highlighter is created when hovering over a
// transform property

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    transform: skew(16deg);',
  '    color: yellow;',
  '  }',
  '</style>',
  'Test the css transform highlighter'
].join("\n");

let test = asyncTest(function*() {
  yield addTab("data:text/html," + PAGE_CONTENT);

  let {view: rView} = yield openRuleView();
  ok(!rView.transformHighlighter, "No highlighter exists in the rule-view (1)");

  info("Faking a mousemove on a non-transform property");
  let {valueSpan} = getRuleViewProperty(rView, "body", "color");
  rView._onMouseMove({target: valueSpan});
  ok(!rView.transformHighlighter, "No highlighter exists in the rule-view (2)");
  ok(!rView.transformHighlighterPromise, "No highlighter is being initialized");

  info("Faking a mousemove on a transform property");
  let {valueSpan} = getRuleViewProperty(rView, "body", "transform");
  rView._onMouseMove({target: valueSpan});
  ok(rView.transformHighlighterPromise, "The highlighter is being initialized");
  let h = yield rView.transformHighlighterPromise;
  is(h, rView.transformHighlighter, "The initialized highlighter is the right one");

  let {view: cView} = yield openComputedView();
  ok(!cView.transformHighlighter, "No highlighter exists in the computed-view (1)");

  info("Faking a mousemove on a non-transform property");
  let {valueSpan} = getComputedViewProperty(cView, "color");
  cView._onMouseMove({target: valueSpan});
  ok(!cView.transformHighlighter, "No highlighter exists in the computed-view (2)");
  ok(!cView.transformHighlighterPromise, "No highlighter is being initialized");

  info("Faking a mousemove on a transform property");
  let {valueSpan} = getComputedViewProperty(cView, "transform");
  cView._onMouseMove({target: valueSpan});
  ok(cView.transformHighlighterPromise, "The highlighter is being initialized");
  let h = yield cView.transformHighlighterPromise;
  is(h, cView.transformHighlighter, "The initialized highlighter is the right one");
});
