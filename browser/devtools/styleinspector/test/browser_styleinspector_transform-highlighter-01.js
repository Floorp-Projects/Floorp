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

const TYPE = "CssTransformHighlighter";

let test = asyncTest(function*() {
  yield addTab("data:text/html," + PAGE_CONTENT);

  let {view: rView} = yield openRuleView();
  let overlay = rView.highlighters;

  ok(!overlay.highlighters[TYPE], "No highlighter exists in the rule-view");
  let h = yield overlay._getHighlighter(TYPE);
  ok(overlay.highlighters[TYPE], "The highlighter has been created in the rule-view");
  is(h, overlay.highlighters[TYPE], "The right highlighter has been created");
  let h2 = yield overlay._getHighlighter(TYPE);
  is(h, h2, "The same instance of highlighter is returned everytime in the rule-view");

  let {view: cView} = yield openComputedView();
  let overlay = cView.highlighters;

  ok(!overlay.highlighters[TYPE], "No highlighter exists in the computed-view");
  let h = yield overlay._getHighlighter(TYPE);
  ok(overlay.highlighters[TYPE], "The highlighter has been created in the computed-view");
  is(h, overlay.highlighters[TYPE], "The right highlighter has been created");
  let h2 = yield overlay._getHighlighter(TYPE);
  is(h, h2, "The same instance of highlighter is returned everytime in the computed-view");
});
