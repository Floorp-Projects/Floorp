/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that disabling a CSS declaration and then removing it from the Rule view
// is tracked as removed only once. Toggling leftover declarations should not introduce
// duplicate changes.

const TEST_URI = `
  <style type='text/css'>
    div {
      color: red;
      background: black;
    }
  </style>
  <div></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();
  const { document: doc, store } = selectChangesView(inspector);

  await selectNode("div", inspector);
  const rule = getRuleViewRuleEditor(ruleView, 1).rule;

  info("Using the second declaration");
  await testRemoveValue(ruleView, store, doc, rule.textProps[1]);
  info("Using the first declaration");
  await testToggle(ruleView, store, doc, rule.textProps[0]);
  info("Using the first declaration");
  await testRemoveName(ruleView, store, doc, rule.textProps[0]);
});

async function testRemoveValue(ruleView, store, doc, prop) {
  info("Test removing disabled declaration by clearing its property value.");
  let onTrackChange;

  onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  info("Disable the declaration");
  await togglePropStatus(ruleView, prop);
  info("Wait for change to be tracked");
  await onTrackChange;

  onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  info("Remove the disabled declaration by clearing its value");
  await setProperty(ruleView, prop, null);
  await onTrackChange;

  const removeDecl = getRemovedDeclarations(doc);
  is(removeDecl.length, 1, "Only one declaration tracked as removed");
}

async function testToggle(ruleView, store, doc, prop) {
  info(
    "Test toggling leftover declaration off and on will not track extra changes."
  );
  let onTrackChange;

  onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  info("Disable the declaration");
  await togglePropStatus(ruleView, prop);
  await onTrackChange;

  onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  info("Re-enable the declaration");
  await togglePropStatus(ruleView, prop);
  await onTrackChange;

  const removeDecl = getRemovedDeclarations(doc);
  is(removeDecl.length, 1, "Still just one declaration tracked as removed");
}

async function testRemoveName(ruleView, store, doc, prop) {
  info("Test removing disabled declaration by clearing its property name.");
  let onTrackChange;

  onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  info("Disable the declaration");
  await togglePropStatus(ruleView, prop);
  await onTrackChange;

  onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  info("Remove the disabled declaration by clearing its name");
  await removeProperty(ruleView, prop);
  await onTrackChange;

  info(`Expecting two declarations removed:
  - one removed by its value in the other test
  - one removed by its name from this test
  `);

  const removeDecl = getRemovedDeclarations(doc);
  is(removeDecl.length, 2, "Two declarations tracked as removed");
  is(removeDecl[0].property, "background", "First declaration name correct");
  is(removeDecl[0].value, "black", "First declaration value correct");
  is(removeDecl[1].property, "color", "Second declaration name correct");
  is(removeDecl[1].value, "red", "Second declaration value correct");
}
