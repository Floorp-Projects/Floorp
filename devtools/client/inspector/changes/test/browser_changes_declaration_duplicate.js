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

  await selectNode("div", inspector);
  await testAddDuplicateDeclarations(ruleView, store, doc);
  await testChangeDuplicateDeclarations(ruleView, store, doc);
  await testRemoveDuplicateDeclarations(ruleView, store, doc);
});

async function testAddDuplicateDeclarations(ruleView, store, doc) {
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

  const addDecl = getAddedDeclarations(doc);
  is(addDecl.length, 2, "Two declarations were tracked as added");
  is(addDecl[0].value, "red", "First declaration has correct property value");
  is(
    addDecl[0].value,
    addDecl[1].value,
    "First and second declarations have identical property values"
  );
}

async function testChangeDuplicateDeclarations(ruleView, store, doc) {
  info(
    "Test that changing one of the duplicate declarations won't change the other"
  );
  const rule = getRuleViewRuleEditor(ruleView, 1).rule;
  const prop = rule.textProps[0];

  info("Change the value of the first of the duplicate declarations");
  const onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  await setProperty(ruleView, prop, "black");
  info("Wait for the change to be tracked");
  await onTrackChange;

  const addDecl = getAddedDeclarations(doc);
  is(addDecl[0].value, "black", "First declaration has changed property value");
  is(
    addDecl[1].value,
    "red",
    "Second declaration has not changed property value"
  );
}

async function testRemoveDuplicateDeclarations(ruleView, store, doc) {
  info(`Test that removing the first of the duplicate declarations
        will not remove the second.`);
  const rule = getRuleViewRuleEditor(ruleView, 1).rule;
  const prop = rule.textProps[0];

  info("Remove first declaration");
  const onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  await removeProperty(ruleView, prop);
  info("Wait for the change to be tracked");
  await onTrackChange;

  const addDecl = getAddedDeclarations(doc);
  const removeDecl = getRemovedDeclarations(doc);
  // Expect no remove operation tracked because it cancels out the original add operation.
  is(removeDecl.length, 0, "No declaration was tracked as removed");
  is(addDecl.length, 1, "Just one declaration left tracked as added");
  is(
    addDecl[0].value,
    "red",
    "Leftover declaration has property value of the former second declaration"
  );
}
