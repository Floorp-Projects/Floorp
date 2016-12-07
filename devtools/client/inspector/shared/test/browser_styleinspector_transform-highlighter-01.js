/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the css transform highlighter is created only when asked and only one
// instance exists across the inspector

const TEST_URI = `
  <style type="text/css">
    body {
      transform: skew(16deg);
    }
  </style>
  Test the css transform highlighter
`;

const TYPE = "CssTransformHighlighter";

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();

  let overlay = view.highlighters;

  ok(!overlay.highlighters[TYPE], "No highlighter exists in the rule-view");
  let h = yield overlay._getHighlighter(TYPE);
  ok(overlay.highlighters[TYPE],
    "The highlighter has been created in the rule-view");
  is(h, overlay.highlighters[TYPE], "The right highlighter has been created");
  let h2 = yield overlay._getHighlighter(TYPE);
  is(h, h2,
    "The same instance of highlighter is returned everytime in the rule-view");

  let onComputedViewReady = inspector.once("computed-view-refreshed");
  let cView = selectComputedView(inspector);
  yield onComputedViewReady;
  overlay = cView.highlighters;

  ok(overlay.highlighters[TYPE], "The highlighter exists in the computed-view");
  let h3 = yield overlay._getHighlighter(TYPE);
  is(h, h3, "The same instance of highlighter is returned everytime " +
    "in the computed-view");
});
