/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that adding duplicate declarations to the Rule view is shown in the Changes panel.

const TEST_URI = `
  <style type='text/css'>
    div {
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
  await testAddDuplicateDeclarations(ruleView, store, panel);
  await testChangeDuplicateDeclarations(ruleView, store, panel);
  await testRemoveDuplicateDeclarations(ruleView, store, panel);
});

async function testAddDuplicateDeclarations(ruleView, store, panel) {
  info(`Test that adding declarations with the same property name and value
        are both tracked.`);

  let onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  info("Add CSS declaration");
  await addProperty(ruleView, 1, "color", "red");
  info("Wait for the change to be tracked");
  await onTrackChange;

  onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  info("Add duplicate CSS declaration");
  await addProperty(ruleView, 1, "color", "red");
  info("Wait for the change to be tracked");
  await onTrackChange;

  const addDecl = panel.querySelectorAll(".declaration.diff-add");
  is(addDecl.length, 2, "Two declarations were tracked as added");
  is(addDecl.item(0).querySelector(".declaration-value").textContent, "red",
     "First declaration has correct property value"
  );
  is(addDecl.item(0).querySelector(".declaration-value").textContent,
     addDecl.item(1).querySelector(".declaration-value").textContent,
     "First and second declarations have identical property values"
  );
}

async function testChangeDuplicateDeclarations(ruleView, store, panel) {
  info("Test that changing one of the duplicate declarations won't change the other");
  const rule = getRuleViewRuleEditor(ruleView, 1).rule;
  const prop = rule.textProps[0];

  info("Change the value of the first of the duplicate declarations");
  const onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  await setProperty(ruleView, prop, "black");
  info("Wait for the change to be tracked");
  await onTrackChange;

  const addDecl = panel.querySelectorAll(".declaration.diff-add");
  is(addDecl.item(0).querySelector(".declaration-value").textContent, "black",
     "First declaration has changed property value"
  );
  is(addDecl.item(1).querySelector(".declaration-value").textContent, "red",
     "Second declaration has not changed property value"
  );
}

async function testRemoveDuplicateDeclarations(ruleView, store, panel) {
  info(`Test that removing the first of the duplicate declarations
        will not remove the second.`);
  const rule = getRuleViewRuleEditor(ruleView, 1).rule;
  const prop = rule.textProps[0];

  info("Remove first declaration");
  const onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  await removeProperty(ruleView, prop);
  info("Wait for the change to be tracked");
  await onTrackChange;

  const addDecl = panel.querySelectorAll(".declaration.diff-add");
  const removeDecl = panel.querySelectorAll(".declaration.diff-remove");
  // Expect no remove operation tracked because it cancels out the original add operation.
  is(removeDecl.length, 0, "No declaration was tracked as removed");
  is(addDecl.length, 1, "Just one declaration left tracked as added");
  is(addDecl.item(0).querySelector(".declaration-value").textContent, "red",
     "Leftover declaration has property value of the former second declaration"
  );
}
