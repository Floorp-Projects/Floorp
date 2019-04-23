/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that renaming the selector of a CSS rule is tracked.
// Expect a selector removal followed by a selector addition and no changed declarations

const TEST_URI = `
  <style type='text/css'>
    div {
      color: red;
    }
  </style>
  <div></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();
  const { document: doc, store } = selectChangesView(inspector);
  const panel = doc.querySelector("#sidebar-panel-changes");

  await selectNode("div", inspector);
  const ruleEditor = getRuleViewRuleEditor(ruleView, 1);

  info("Focusing the first rule's selector name in the Rule view");
  const editor = await focusEditableField(ruleView, ruleEditor.selectorText);
  info("Entering a new selector name");
  editor.input.value = ".test";

  // Expect two "TRACK_CHANGE" actions: one for removal, one for addition.
  const onTrackChange = waitUntilAction(store, "TRACK_CHANGE", 2);
  const onRuleViewChanged = once(ruleView, "ruleview-changed");
  info("Pressing Enter key to commit the change");
  EventUtils.synthesizeKey("KEY_Enter");
  info("Waiting for rule view to update");
  await onRuleViewChanged;
  info("Wait for the change to be tracked");
  await onTrackChange;

  const rules = panel.querySelectorAll(".changes__rule");
  is(rules.length, 1, "One rule was tracked as changed");

  info("Expect old selector to be removed and new selector to be added");
  const addedSelectors = getAddedSelectors(panel);
  const removedSelectors = getRemovedSelectors(panel);
  is(addedSelectors.length, 1, "One new selector was tracked as added");
  is(addedSelectors.item(0).title, ".test", "New selector is correct");
  is(removedSelectors.length, 1, "One old selector was tracked as removed");
  is(removedSelectors.item(0).title, "div", "Old selector is correct");

  info("Expect no declarations to have been added or removed during selector change");
  const removeDecl = getRemovedDeclarations(doc, rules.item(0));
  is(removeDecl.length, 0, "No declarations removed");
  const addDecl = getAddedDeclarations(doc, rules.item(0));
  is(addDecl.length, 0, "No declarations added");
});
