/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the css transform highlighter is created when hovering over a
// transform property

const TEST_URI = `
  <style type="text/css">
    body {
      transform: skew(16deg);
      color: yellow;
    }
  </style>
  Test the css transform highlighter
`;

var TYPE = "CssTransformHighlighter";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  let hs = view.highlighters;

  ok(!hs.highlighters[TYPE], "No highlighter exists in the rule-view (1)");

  info("Faking a mousemove on a non-transform property");
  let {valueSpan} = getRuleViewProperty(view, "body", "color");
  hs.onMouseMove({target: valueSpan});
  ok(!hs.highlighters[TYPE], "No highlighter exists in the rule-view (2)");

  info("Faking a mousemove on a transform property");
  ({valueSpan} = getRuleViewProperty(view, "body", "transform"));
  let onHighlighterShown = hs.once("highlighter-shown");
  hs.onMouseMove({target: valueSpan});
  await onHighlighterShown;

  const onComputedViewReady = inspector.once("computed-view-refreshed");
  const cView = selectComputedView(inspector);
  await onComputedViewReady;
  hs = cView.highlighters;

  info("Remove the created transform highlighter");
  hs.highlighters[TYPE].finalize();
  hs.highlighters[TYPE] = null;

  info("Faking a mousemove on a non-transform property");
  ({valueSpan} = getComputedViewProperty(cView, "color"));
  hs.onMouseMove({target: valueSpan});
  ok(!hs.highlighters[TYPE], "No highlighter exists in the computed-view (3)");

  info("Faking a mousemove on a transform property");
  ({valueSpan} = getComputedViewProperty(cView, "transform"));
  onHighlighterShown = hs.once("highlighter-shown");
  hs.onMouseMove({target: valueSpan});
  await onHighlighterShown;

  ok(hs.highlighters[TYPE],
    "The highlighter has been created in the computed-view");
});
