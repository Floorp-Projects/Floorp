/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the selector highlighter is removed when modifying a selector and
// the selector highlighter works for the newly added unmatched rule.

const TEST_URI = `
  <style type="text/css">
    p {
      background: red;
    }
  </style>
  <p>Test the selector highlighter</p>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("p", inspector);

  ok(!view.selectorHighlighter,
    "No selectorhighlighter exist in the rule-view");

  await testSelectorHighlight(view, "p");
  await testEditSelector(view, "body");
  await testSelectorHighlight(view, "body");
});

async function testSelectorHighlight(view, name) {
  info("Test creating selector highlighter");

  info("Clicking on a selector icon");
  const icon = await getRuleViewSelectorHighlighterIcon(view, name);

  const onToggled = view.once("ruleview-selectorhighlighter-toggled");
  EventUtils.synthesizeMouseAtCenter(icon, {}, view.styleWindow);
  const isVisible = await onToggled;

  ok(view.selectorHighlighter, "The selectorhighlighter instance was created");
  ok(isVisible, "The toggle event says the highlighter is visible");
}

async function testEditSelector(view, name) {
  info("Test editing existing selector fields");

  const ruleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing an existing selector name in the rule-view");
  const editor = await focusEditableField(view, ruleEditor.selectorText);

  is(inplaceEditor(ruleEditor.selectorText), editor,
    "The selector editor got focused");

  info("Waiting for rule view to update");
  const onToggled = view.once("ruleview-selectorhighlighter-toggled");

  info("Entering a new selector name and committing");
  editor.input.value = name;
  EventUtils.synthesizeKey("KEY_Enter");

  const isVisible = await onToggled;

  ok(!view.highlighters.selectorHighlighterShown,
    "The selectorHighlighterShown instance was removed");
  ok(!isVisible, "The toggle event says the highlighter is not visible");
}
