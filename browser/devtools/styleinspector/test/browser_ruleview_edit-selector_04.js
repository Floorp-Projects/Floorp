/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the selector highlighter is removed when modifying a selector and
// the selector highlighter works for the newly added unmatched rule.

const TEST_URI = [
  '<style type="text/css">',
  '  p {',
  '    background: red;',
  '  }',
  '</style>',
  '<p>Test the selector highlighter</p>'
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();

  ok(!view.selectorHighlighter, "No selectorhighlighter exist in the rule-view");

  yield selectNode("p", inspector);
  yield testSelectorHighlight(view, "p");
  yield testEditSelector(view, "body");
  yield testSelectorHighlight(view, "body");
});

function* testSelectorHighlight(view, name) {
  info("Test creating selector highlighter");

  info("Clicking on a selector icon");
  let icon = getRuleViewSelectorHighlighterIcon(view, name);

  let onToggled = view.once("ruleview-selectorhighlighter-toggled");
  EventUtils.synthesizeMouseAtCenter(icon, {}, view.styleWindow);
  let isVisible = yield onToggled;

  ok(view.selectorHighlighter, "The selectorhighlighter instance was created");
  ok(isVisible, "The toggle event says the highlighter is visible");
}

function* testEditSelector(view, name) {
  info("Test editing existing selector fields");

  let ruleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing an existing selector name in the rule-view");
  let editor = yield focusEditableField(view, ruleEditor.selectorText);

  is(inplaceEditor(ruleEditor.selectorText), editor,
    "The selector editor got focused");

  info("Waiting for rule view to update");
  let onToggled = view.once("ruleview-selectorhighlighter-toggled");

  info("Entering a new selector name and committing");
  editor.input.value = name;
  EventUtils.synthesizeKey("VK_RETURN", {});

  let isVisible = yield onToggled;

  ok(!view.highlightedSelector, "The selectorhighlighter instance was removed");
  ok(!isVisible, "The toggle event says the highlighter is not visible");
}
