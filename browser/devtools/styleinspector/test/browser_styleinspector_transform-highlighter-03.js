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

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();

  // Mock the highlighter front to get the reference of the NodeFront
  let HighlighterFront = {
    isShown: false,
    nodeFront: null,
    nbOfTimesShown: 0,
    show: function(nodeFront) {
      this.nodeFront = nodeFront;
      this.isShown = true;
      this.nbOfTimesShown ++;
      return promise.resolve(true);
    },
    hide: function() {
      this.nodeFront = null;
      this.isShown = false;
      return promise.resolve();
    }
  };

  // Inject the mock highlighter in the rule-view
  let hs = view.highlighters;
  hs.promises[TYPE] = promise.resolve(HighlighterFront);

  let {valueSpan} = getRuleViewProperty(view, "body", "transform");

  info("Checking that the HighlighterFront's show/hide methods are called");
  let onHighlighterShown = hs.once("highlighter-shown");
  hs._onMouseMove({target: valueSpan});
  yield onHighlighterShown;
  ok(HighlighterFront.isShown, "The highlighter is shown");
  let onHighlighterHidden = hs.once("highlighter-hidden");
  hs._onMouseLeave();
  yield onHighlighterHidden;
  ok(!HighlighterFront.isShown, "The highlighter is hidden");

  info("Checking that hovering several times over the same property doesn't" +
    " show the highlighter several times");
  let nb = HighlighterFront.nbOfTimesShown;
  onHighlighterShown = hs.once("highlighter-shown");
  hs._onMouseMove({target: valueSpan});
  yield onHighlighterShown;
  is(HighlighterFront.nbOfTimesShown, nb + 1, "The highlighter was shown once");
  hs._onMouseMove({target: valueSpan});
  hs._onMouseMove({target: valueSpan});
  is(HighlighterFront.nbOfTimesShown, nb + 1,
    "The highlighter was shown once, after several mousemove");

  info("Checking that the right NodeFront reference is passed");
  yield selectNode("html", inspector);
  ({valueSpan} = getRuleViewProperty(view, "html", "transform"));
  onHighlighterShown = hs.once("highlighter-shown");
  hs._onMouseMove({target: valueSpan});
  yield onHighlighterShown;
  is(HighlighterFront.nodeFront.tagName, "HTML",
    "The right NodeFront is passed to the highlighter (1)");

  yield selectNode("body", inspector);
  ({valueSpan} = getRuleViewProperty(view, "body", "transform"));
  onHighlighterShown = hs.once("highlighter-shown");
  hs._onMouseMove({target: valueSpan});
  yield onHighlighterShown;
  is(HighlighterFront.nodeFront.tagName, "BODY",
    "The right NodeFront is passed to the highlighter (2)");

  info("Checking that the highlighter gets hidden when hovering a " +
    "non-transform property");
  ({valueSpan} = getRuleViewProperty(view, "body", "color"));
  onHighlighterHidden = hs.once("highlighter-hidden");
  hs._onMouseMove({target: valueSpan});
  yield onHighlighterHidden;
  ok(!HighlighterFront.isShown, "The highlighter is hidden");
});
