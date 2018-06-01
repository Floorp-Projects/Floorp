/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the css transform highlighter is shown when hovering over transform
// properties

// Note that in this test, we mock the highlighter front, merely testing the
// behavior of the style-inspector UI for now

const TEST_URI = `
  <style type="text/css">
    html {
      transform: scale(.9);
    }
    body {
      transform: skew(16deg);
      color: purple;
    }
  </style>
  Test the css transform highlighter
`;

const TYPE = "CssTransformHighlighter";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();

  // Mock the highlighter front to get the reference of the NodeFront
  const HighlighterFront = {
    isShown: false,
    nodeFront: null,
    nbOfTimesShown: 0,
    show: function(nodeFront) {
      this.nodeFront = nodeFront;
      this.isShown = true;
      this.nbOfTimesShown++;
      return promise.resolve(true);
    },
    hide: function() {
      this.nodeFront = null;
      this.isShown = false;
      return promise.resolve();
    },
    finalize: function() {}
  };

  // Inject the mock highlighter in the rule-view
  const hs = view.highlighters;
  hs.highlighters[TYPE] = HighlighterFront;

  let {valueSpan} = getRuleViewProperty(view, "body", "transform");

  info("Checking that the HighlighterFront's show/hide methods are called");
  let onHighlighterShown = hs.once("highlighter-shown");
  hs.onMouseMove({target: valueSpan});
  await onHighlighterShown;
  ok(HighlighterFront.isShown, "The highlighter is shown");
  let onHighlighterHidden = hs.once("highlighter-hidden");
  hs.onMouseOut();
  await onHighlighterHidden;
  ok(!HighlighterFront.isShown, "The highlighter is hidden");

  info("Checking that hovering several times over the same property doesn't" +
    " show the highlighter several times");
  const nb = HighlighterFront.nbOfTimesShown;
  onHighlighterShown = hs.once("highlighter-shown");
  hs.onMouseMove({target: valueSpan});
  await onHighlighterShown;
  is(HighlighterFront.nbOfTimesShown, nb + 1, "The highlighter was shown once");
  hs.onMouseMove({target: valueSpan});
  hs.onMouseMove({target: valueSpan});
  is(HighlighterFront.nbOfTimesShown, nb + 1,
    "The highlighter was shown once, after several mousemove");

  info("Checking that the right NodeFront reference is passed");
  await selectNode("html", inspector);
  ({valueSpan} = getRuleViewProperty(view, "html", "transform"));
  onHighlighterShown = hs.once("highlighter-shown");
  hs.onMouseMove({target: valueSpan});
  await onHighlighterShown;
  is(HighlighterFront.nodeFront.tagName, "HTML",
    "The right NodeFront is passed to the highlighter (1)");

  await selectNode("body", inspector);
  ({valueSpan} = getRuleViewProperty(view, "body", "transform"));
  onHighlighterShown = hs.once("highlighter-shown");
  hs.onMouseMove({target: valueSpan});
  await onHighlighterShown;
  is(HighlighterFront.nodeFront.tagName, "BODY",
    "The right NodeFront is passed to the highlighter (2)");

  info("Checking that the highlighter gets hidden when hovering a " +
    "non-transform property");
  ({valueSpan} = getRuleViewProperty(view, "body", "color"));
  onHighlighterHidden = hs.once("highlighter-hidden");
  hs.onMouseMove({target: valueSpan});
  await onHighlighterHidden;
  ok(!HighlighterFront.isShown, "The highlighter is hidden");
});
