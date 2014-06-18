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

let TYPE = "CssTransformHighlighter";

let test = asyncTest(function*() {
  yield addTab("data:text/html," + PAGE_CONTENT);


  let {view: rView} = yield openRuleView();
  let hs = rView.highlighters;

  ok(!hs.highlighters[TYPE], "No highlighter exists in the rule-view (1)");
  ok(!hs.promises[TYPE], "No highlighter is being created in the rule-view (1)");

  info("Faking a mousemove on a non-transform property");
  let {valueSpan} = getRuleViewProperty(rView, "body", "color");
  hs._onMouseMove({target: valueSpan});
  ok(!hs.highlighters[TYPE], "No highlighter exists in the rule-view (2)");
  ok(!hs.promises[TYPE], "No highlighter is being created in the rule-view (2)");

  info("Faking a mousemove on a transform property");
  let {valueSpan} = getRuleViewProperty(rView, "body", "transform");
  hs._onMouseMove({target: valueSpan});
  ok(hs.promises[TYPE], "The highlighter is being initialized");
  let h = yield hs.promises[TYPE];
  is(h, hs.highlighters[TYPE], "The initialized highlighter is the right one");

  let {view: cView} = yield openComputedView();
  let hs = cView.highlighters;

  ok(!hs.highlighters[TYPE], "No highlighter exists in the computed-view (1)");
  ok(!hs.promises[TYPE], "No highlighter is being created in the computed-view (1)");

  info("Faking a mousemove on a non-transform property");
  let {valueSpan} = getComputedViewProperty(cView, "color");
  hs._onMouseMove({target: valueSpan});
  ok(!hs.highlighters[TYPE], "No highlighter exists in the computed-view (2)");
  ok(!hs.promises[TYPE], "No highlighter is being created in the computed-view (2)");

  info("Faking a mousemove on a transform property");
  let {valueSpan} = getComputedViewProperty(cView, "transform");
  hs._onMouseMove({target: valueSpan});
  ok(hs.promises[TYPE], "The highlighter is being initialized");
  let h = yield hs.promises[TYPE];
  is(h, hs.highlighters[TYPE], "The initialized highlighter is the right one");
});
