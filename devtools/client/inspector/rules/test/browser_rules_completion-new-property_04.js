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

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view} = await openRuleView();

  info("Selecting the test node");
  await selectNode("h1", inspector);

  info("Focusing the new property editable field");
  const ruleEditor = getRuleViewRuleEditor(view, 1);
  let editor = await focusNewRuleViewProperty(ruleEditor);

  info("Sending \"background\" to the editable field.");
  for (const key of "background") {
    const onSuggest = editor.once("after-suggest");
    EventUtils.synthesizeKey(key, {}, view.styleWindow);
    await onSuggest;
  }

  const itemIndex = 4;
  const bgcItem = editor.popup.getItemAtIndex(itemIndex);
  is(bgcItem.label, "background-color",
    "Check the expected completion element is background-color.");
  editor.popup.selectedIndex = itemIndex;

  info("Select the background-color suggestion with a mouse click.");
  const onSuggest = editor.once("after-suggest");
  const node = editor.popup.elements.get(bgcItem);
  EventUtils.synthesizeMouseAtCenter(node, {}, editor.popup._window);

  await onSuggest;
  is(editor.input.value, "background-color", "Correct value is autocompleted");

  info("Press RETURN to move the focus to a property value editor.");
  let onModifications = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);

  await onModifications;

  // Getting the new value editor after focus
  editor = inplaceEditor(view.styleDocument.activeElement);
  const textProp = ruleEditor.rule.textProps[1];

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
  await onModifications;

  is(textProp.value, "#F00", "Text prop should have been changed.");
});
