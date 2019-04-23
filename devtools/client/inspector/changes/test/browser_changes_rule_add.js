/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that adding a new CSS rule in the Rules view is tracked in the Changes panel.
// Renaming the selector of the new rule should overwrite the tracked selector.

const TEST_URI = `
  <div></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();
  const { document: doc, store } = selectChangesView(inspector);
  const panel = doc.querySelector("#sidebar-panel-changes");

  await selectNode("div", inspector);
  await testTrackAddNewRule(store, inspector, ruleView, panel);
  await testTrackRenameNewRule(store, inspector, ruleView, panel);
});

async function testTrackAddNewRule(store, inspector, ruleView, panel) {
  const onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  info("Adding a new CSS rule in the Rule view");
  await addNewRule(inspector, ruleView);
  info("Pressing escape to leave the editor");
  EventUtils.synthesizeKey("KEY_Escape");
  info("Waiting for changes to be tracked");
  await onTrackChange;

  const addedSelectors = getAddedSelectors(panel);
  const removedSelectors = getRemovedSelectors(panel);
  is(addedSelectors.length, 1, "One selector was tracked as added");
  is(addedSelectors.item(0).title, "div", "New rule's has DIV selector");
  is(removedSelectors.length, 0, "No selectors tracked as removed");
}

async function testTrackRenameNewRule(store, inspector, ruleView, panel) {
  info("Focusing the first rule's selector name in the Rule view");
  const ruleEditor = getRuleViewRuleEditor(ruleView, 1);
  const editor = await focusEditableField(ruleView, ruleEditor.selectorText);
  info("Entering a new selector name");
  editor.input.value = ".test";

  // Expect two "TRACK_CHANGE" actions: one for removal, one for addition.
  const onTrackChange = waitUntilAction(store, "TRACK_CHANGE", 2);
  const onRuleViewChanged = once(ruleView, "ruleview-changed");
  EventUtils.synthesizeKey("KEY_Enter");
  await onRuleViewChanged;
  info("Waiting for changes to be tracked");
  await onTrackChange;

  const addedSelectors = getAddedSelectors(panel);
  const removedSelectors = getRemovedSelectors(panel);
  is(addedSelectors.length, 1, "One selector was tracked as added");
  is(addedSelectors.item(0).title, ".test", "New rule's selector was updated in place.");
  is(removedSelectors.length, 0, "No selectors tracked as removed");
}
