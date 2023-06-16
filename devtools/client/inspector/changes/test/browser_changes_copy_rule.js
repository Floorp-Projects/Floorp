/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the Changes panel Copy Rule button and context menu will populate the
// clipboard with the entire contents of the changed rule, including unchanged properties.

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
  div {
    color: green;
    margin: 0;
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

  info("Click the Copy Rule button and expect the changed rule on clipboard");
  const button = panelDoc.querySelector(".changes__copy-rule-button");
  await waitForClipboardPromise(
    () => button.click(),
    () => checkClipboardData(EXPECTED_CLIPBOARD)
  );

  emptyClipboard();

  info(
    "Click the Copy Rule context menu item and expect the changed rule on the clipboard"
  );
  const addDecl = getAddedDeclarations(panelDoc);
  const menu = await getChangesContextMenu(changesView, addDecl[0].element);
  const menuItem = menu.items.find(
    item => item.id === "changes-contextmenu-copy-rule"
  );
  await waitForClipboardPromise(
    () => menuItem.click(),
    () => checkClipboardData(EXPECTED_CLIPBOARD)
  );
});

function checkClipboardData(expected) {
  const actual = SpecialPowers.getClipboardData("text/plain");
  return actual.trim() === expected.trim();
}
