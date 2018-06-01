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

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();

  // Mock the highlighter front to get the reference of the NodeFront
  const HighlighterFront = {
    isShown: false,
    nodeFront: null,
    options: null,
    show: function(nodeFront, options) {
      this.nodeFront = nodeFront;
      this.options = options;
      this.isShown = true;
    },
    hide: function() {
      this.nodeFront = null;
      this.options = null;
      this.isShown = false;
    }
  };

  // Inject the mock highlighter in the rule-view
  view.selectorHighlighter = HighlighterFront;

  let icon = await getRuleViewSelectorHighlighterIcon(view, "body");

  info("Checking that the HighlighterFront's show/hide methods are called");

  info("Clicking once on the body selector highlighter icon");
  await clickSelectorIcon(icon, view);
  ok(HighlighterFront.isShown, "The highlighter is shown");

  info("Clicking once again on the body selector highlighter icon");
  await clickSelectorIcon(icon, view);
  ok(!HighlighterFront.isShown, "The highlighter is hidden");

  info("Checking that the right NodeFront reference and options are passed");
  await selectNode("p", inspector);
  icon = await getRuleViewSelectorHighlighterIcon(view, "p");

  await clickSelectorIcon(icon, view);
  is(HighlighterFront.nodeFront.tagName, "P",
    "The right NodeFront is passed to the highlighter (1)");
  is(HighlighterFront.options.selector, "p",
    "The right selector option is passed to the highlighter (1)");

  await selectNode("body", inspector);
  icon = await getRuleViewSelectorHighlighterIcon(view, "body");
  await clickSelectorIcon(icon, view);
  is(HighlighterFront.nodeFront.tagName, "BODY",
    "The right NodeFront is passed to the highlighter (2)");
  is(HighlighterFront.options.selector, "body",
    "The right selector option is passed to the highlighter (2)");
});
