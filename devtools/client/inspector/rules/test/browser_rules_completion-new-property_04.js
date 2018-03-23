/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a new property editor supports the following flow:
// - type first character of property name
// - select an autocomplete suggestion !!with a mouse click!!
// - press RETURN to move to the property value
// - blur the input to commit

const TEST_URI = "<style>.title {color: red;}</style>" +
                 "<h1 class=title>Header</h1>";

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let { inspector, view} = yield openRuleView();

  info("Selecting the test node");
  yield selectNode("h1", inspector);

  info("Focusing the new property editable field");
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let editor = yield focusNewRuleViewProperty(ruleEditor);

  info("Sending \"background\" to the editable field.");
  for (let key of "background") {
    let onSuggest = editor.once("after-suggest");
    EventUtils.synthesizeKey(key, {}, view.styleWindow);
    yield onSuggest;
  }

  const itemIndex = 4;
  let bgcItem = editor.popup.getItemAtIndex(itemIndex);
  is(bgcItem.label, "background-color",
    "Check the expected completion element is background-color.");
  editor.popup.selectedIndex = itemIndex;

  info("Select the background-color suggestion with a mouse click.");
  let onSuggest = editor.once("after-suggest");
  let node = editor.popup.elements.get(bgcItem);
  EventUtils.synthesizeMouseAtCenter(node, {}, editor.popup._window);

  yield onSuggest;
  is(editor.input.value, "background-color", "Correct value is autocompleted");

  info("Press RETURN to move the focus to a property value editor.");
  let onModifications = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);

  yield onModifications;

  // Getting the new value editor after focus
  editor = inplaceEditor(view.styleDocument.activeElement);
  let textProp = ruleEditor.rule.textProps[1];

  is(ruleEditor.rule.textProps.length, 2,
    "Created a new text property.");
  is(ruleEditor.propertyList.children.length, 2,
    "Created a property editor.");
  is(editor, inplaceEditor(textProp.editor.valueSpan),
    "Editing the value span now.");

  info("Entering a value and blurring the field to expect a rule change");
  editor.input.value = "#F00";

  onModifications = view.once("ruleview-changed");
  editor.input.blur();
  yield onModifications;

  is(textProp.value, "#F00", "Text prop should have been changed.");
});
