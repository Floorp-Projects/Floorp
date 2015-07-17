/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the selector highlighter toggling mechanism works correctly.

// Note that in this test, we mock the highlighter front, merely testing the
// behavior of the style-inspector UI for now

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  div {text-decoration: underline;}',
  '  .node-1 {color: red;}',
  '  .node-2 {color: green;}',
  '</style>',
  '<div class="node-1">Node 1</div>',
  '<div class="node-2">Node 2</div>'
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + PAGE_CONTENT);

  let {inspector, view} = yield openRuleView();

  // Mock the highlighter front.
  let HighlighterFront = {
    isShown: false,
    show: function() {
      this.isShown = true;
    },
    hide: function() {
      this.isShown = false;
    }
  };

  // Inject the mock highlighter in the rule-view
  view.selectorHighlighter = HighlighterFront;

  info("Select .node-1 and click on the .node-1 selector icon");
  yield selectNode(".node-1", inspector);
  let icon = getRuleViewSelectorHighlighterIcon(view, ".node-1");
  yield clickSelectorIcon(icon, view);
  ok(HighlighterFront.isShown, "The highlighter is shown");

  info("With .node-1 still selected, click again on the .node-1 selector icon");
  yield clickSelectorIcon(icon, view);
  ok(!HighlighterFront.isShown, "The highlighter is now hidden");

  info("With .node-1 still selected, click on the div selector icon");
  icon = getRuleViewSelectorHighlighterIcon(view, "div");
  yield clickSelectorIcon(icon, view);
  ok(HighlighterFront.isShown, "The highlighter is shown again");

  info("With .node-1 still selected, click again on the .node-1 selector icon");
  icon = getRuleViewSelectorHighlighterIcon(view, ".node-1");
  yield clickSelectorIcon(icon, view);
  ok(HighlighterFront.isShown,
    "The highlighter is shown again since the clicked selector was different");

  info("Selecting .node-2");
  yield selectNode(".node-2", inspector);
  ok(HighlighterFront.isShown, "The highlighter is still shown after selection");

  info("With .node-2 selected, click on the div selector icon");
  icon = getRuleViewSelectorHighlighterIcon(view, "div");
  yield clickSelectorIcon(icon, view);
  ok(HighlighterFront.isShown,
    "The highlighter is shown still since the selected was different");

  info("Switching back to .node-1 and clicking on the div selector");
  yield selectNode(".node-1", inspector);
  icon = getRuleViewSelectorHighlighterIcon(view, "div");
  yield clickSelectorIcon(icon, view);
  ok(!HighlighterFront.isShown,
    "The highlighter is hidden now that the same selector was clicked");
});

function* clickSelectorIcon(icon, view) {
  let onToggled = view.once("ruleview-selectorhighlighter-toggled");
  EventUtils.synthesizeMouseAtCenter(icon, {}, view.styleWindow);
  yield onToggled;
}
