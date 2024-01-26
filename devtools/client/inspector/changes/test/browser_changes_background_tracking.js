/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that CSS changes are collected in the background without the Changes panel visible

const TEST_URI = `
  <style type='text/css'>
    div {
      color: red;
    }
  </style>
  <div></div>
`;

add_task(async function () {
  info("Ensure Changes panel is NOT the default panel; use Computed panel");
  await pushPref("devtools.inspector.activeSidebar", "computedview");

  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();

  await selectNode("div", inspector);
  const prop = getTextProperty(ruleView, 1, { color: "red" });

  info("Disable the first CSS declaration");
  await togglePropStatus(ruleView, prop);

  info("Select the Changes panel");
  const { document: doc, store } = selectChangesView(inspector);
  const onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  const onResetChanges = waitForDispatch(store, "RESET_CHANGES");

  info("Wait for change to be tracked");
  await onTrackChange;
  const removedDeclarations = getRemovedDeclarations(doc);
  is(removedDeclarations.length, 1, "One declaration was tracked as removed");

  // Test for Bug 1656477. Check that changes are not cleared immediately afterwards.
  info("Wait to see if the RESET_CHANGES action is dispatched unexpecteldy");
  const sym = Symbol();
  const onTimeout = wait(500).then(() => sym);
  const raceResult = await Promise.any([onResetChanges, onTimeout]);
  Assert.strictEqual(raceResult, sym, "RESET_CHANGES has not been dispatched");
});
