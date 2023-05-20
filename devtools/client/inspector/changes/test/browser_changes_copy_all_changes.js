/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the Changes panel Copy All Changes button will populate the
// clipboard with a summary of just the changed declarations.

const TEST_URI = `
  <style type='text/css'>
  div {
    color: red;
    margin: 0;
  }
  </style>
  <div></div>
`;

// Indentation is important. A strict check will be done against the clipboard content.
const EXPECTED_CLIPBOARD = `
/* Inline #0 | data:text/html;charset=utf-8,${TEST_URI} */

div {
  /* color: red; */
  color: green;
}
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();
  const changesView = selectChangesView(inspector);
  const { document: panelDoc, store } = changesView;

  await selectNode("div", inspector);
  const onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  await updateDeclaration(ruleView, 1, { color: "red" }, { color: "green" });
  await onTrackChange;

  info(
    "Check that clicking the Copy All Changes button copies all changes to the clipboard."
  );
  const button = panelDoc.querySelector(".changes__copy-all-changes-button");
  await waitForClipboardPromise(
    () => button.click(),
    () => checkClipboardData(EXPECTED_CLIPBOARD)
  );
});

function checkClipboardData(expected) {
  const actual = SpecialPowers.getClipboardData("text/plain");
  return decodeURIComponent(actual).trim() === expected.trim();
}
