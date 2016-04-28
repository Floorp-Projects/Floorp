/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the selector highlighter is shown when clicking on a selector icon
// in the rule-view

// Note that in this test, we mock the highlighter front, merely testing the
// behavior of the style-inspector UI for now

const TEST_URI = `
  <style type="text/css">
    body {
      background: red;
    }
    p {
      color: white;
    }
  </style>
  <p>Testing the selector highlighter</p>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();

  // Mock the highlighter front to get the reference of the NodeFront
  let HighlighterFront = {
    isShown: false,
    nodeFront: null,
    options: null,
    show: function (nodeFront, options) {
      this.nodeFront = nodeFront;
      this.options = options;
      this.isShown = true;
    },
    hide: function () {
      this.nodeFront = null;
      this.options = null;
      this.isShown = false;
    }
  };

  // Inject the mock highlighter in the rule-view
  view.selectorHighlighter = HighlighterFront;

  let icon = getRuleViewSelectorHighlighterIcon(view, "body");

  info("Checking that the HighlighterFront's show/hide methods are called");

  info("Clicking once on the body selector highlighter icon");
  yield clickSelectorIcon(icon, view);
  ok(HighlighterFront.isShown, "The highlighter is shown");

  info("Clicking once again on the body selector highlighter icon");
  yield clickSelectorIcon(icon, view);
  ok(!HighlighterFront.isShown, "The highlighter is hidden");

  info("Checking that the right NodeFront reference and options are passed");
  yield selectNode("p", inspector);
  icon = getRuleViewSelectorHighlighterIcon(view, "p");

  yield clickSelectorIcon(icon, view);
  is(HighlighterFront.nodeFront.tagName, "P",
    "The right NodeFront is passed to the highlighter (1)");
  is(HighlighterFront.options.selector, "p",
    "The right selector option is passed to the highlighter (1)");

  yield selectNode("body", inspector);
  icon = getRuleViewSelectorHighlighterIcon(view, "body");
  yield clickSelectorIcon(icon, view);
  is(HighlighterFront.nodeFront.tagName, "BODY",
    "The right NodeFront is passed to the highlighter (2)");
  is(HighlighterFront.options.selector, "body",
    "The right selector option is passed to the highlighter (2)");
});

function* clickSelectorIcon(icon, view) {
  let onToggled = view.once("ruleview-selectorhighlighter-toggled");
  EventUtils.synthesizeMouseAtCenter(icon, {}, view.styleWindow);
  yield onToggled;
}
