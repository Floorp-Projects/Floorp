/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that commented properties can be added and are disabled.

const TEST_URI = "<div id='testid'></div>";

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testCreateNewSetOfCommentedAndUncommentedProperties(view);
});

function* testCreateNewSetOfCommentedAndUncommentedProperties(view) {
  info("Test creating a new set of commented and uncommented properties");

  info("Focusing a new property name in the rule-view");
  let ruleEditor = getRuleViewRuleEditor(view, 0);
  let editor = yield focusEditableField(view, ruleEditor.closeBrace);
  is(inplaceEditor(ruleEditor.newPropSpan), editor,
    "The new property editor has focus");

  info(
    "Entering a commented property/value pair into the property name editor");
  let input = editor.input;
  input.value = `color: blue;
                 /* background-color: yellow; */
                 width: 200px;
                 height: 100px;
                 /* padding-bottom: 1px; */`;

  info("Pressing return to commit and focus the new value field");
  let onModifications = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  yield onModifications;

  let textProps = ruleEditor.rule.textProps;
  ok(textProps[0].enabled, "The 'color' property is enabled.");
  ok(!textProps[1].enabled, "The 'background-color' property is disabled.");
  ok(textProps[2].enabled, "The 'width' property is enabled.");
  ok(textProps[3].enabled, "The 'height' property is enabled.");
  ok(!textProps[4].enabled, "The 'padding-bottom' property is disabled.");
}
