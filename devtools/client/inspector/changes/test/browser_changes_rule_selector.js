/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that renaming the selector of a CSS declaration in the Rule view is tracked as
// one rule removal with the old selector and one rule addition with the new selector.

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

  // Expect two "TRACK_CHANGE" actions: one for rule removal, one for rule addition.
  const onTrackChange = waitUntilAction(store, "TRACK_CHANGE", 2);
  const onRuleViewChanged = once(ruleView, "ruleview-changed");
  info("Pressing Enter key to commit the change");
  EventUtils.synthesizeKey("KEY_Enter");
  info("Waiting for rule view to update");
  await onRuleViewChanged;
  info("Wait for the change to be tracked");
  await onTrackChange;

  const rules = panel.querySelectorAll(".rule");
  is(rules.length, 2, "Two rules were tracked as changed");

  const firstSelector = rules.item(0).querySelector(".selector");
  is(firstSelector.title, "div", "Old selector name was tracked.");
  ok(firstSelector.classList.contains("diff-remove"), "Old selector was removed.");

  const secondSelector = rules.item(1).querySelector(".selector");
  is(secondSelector.title, ".test", "New selector name was tracked.");
  ok(secondSelector.classList.contains("diff-add"), "New selector was added.");

  info("Get removed declarations from first rule");
  const removeDecl = getRemovedDeclarations(doc, rules.item(0));
  is(removeDecl.length, 1, "First rule has correct number of declarations removed");

  info("Get added declarations from second rule");
  const addDecl = getAddedDeclarations(doc, rules.item(1));
  is(addDecl.length, 1, "Second rule has correct number of declarations added");

  info("Checking that the two rules have identical declarations");
  for (let i = 0; i < removeDecl.length; i++) {
    is(removeDecl[i].property, addDecl[i].property,
      `Declaration names match at index ${i}`);
    is(removeDecl[i].value, addDecl[i].value,
      `Declaration values match at index ${i}`);
  }
});
