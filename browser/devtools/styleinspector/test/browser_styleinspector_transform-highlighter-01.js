/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the css transform highlighter is created only when asked

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    transform: skew(16deg);',
  '  }',
  '</style>',
  'Test the css transform highlighter'
].join("\n");

let test = asyncTest(function*() {
  yield addTab("data:text/html," + PAGE_CONTENT);

  let {view: rView} = yield openRuleView();

  ok(!rView.transformHighlighter, "No highlighter exists in the rule-view");
  let h = yield rView.getTransformHighlighter();
  ok(rView.transformHighlighter, "The highlighter has been created in the rule-view");
  is(h, rView.transformHighlighter, "The right highlighter has been created");
  let h2 = yield rView.getTransformHighlighter();
  is(h, h2, "The same instance of highlighter is returned everytime in the rule-view");

  let {view: cView} = yield openComputedView();

  ok(!cView.transformHighlighter, "No highlighter exists in the computed-view");
  let h = yield cView.getTransformHighlighter();
  ok(cView.transformHighlighter, "The highlighter has been created in the computed-view");
  is(h, cView.transformHighlighter, "The right highlighter has been created");
  let h2 = yield cView.getTransformHighlighter();
  is(h, h2, "The same instance of highlighter is returned everytime in the computed-view");
});
