/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the Changes panel Copy Declaration context menu item will populate the
// clipboard with the changed declaration.

const TEST_URI = `
  <style type='text/css'>
  div {
    color: red;
    margin: 0;
  }
  </style>
  <div></div>
`;

const EXPECTED_CLIPBOARD_REMOVED = `/* color: red; */`;
const EXPECTED_CLIPBOARD_ADDED = `color: green;`;

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
    "Click the Copy Declaration context menu item for the removed declaration"
  );
  const removeDecl = getRemovedDeclarations(panelDoc);
  const addDecl = getAddedDeclarations(panelDoc);

  let menu = await getChangesContextMenu(changesView, removeDecl[0].element);
  let menuItem = menu.items.find(
    item => item.id === "changes-contextmenu-copy-declaration"
  );
  await waitForClipboardPromise(
    () => menuItem.click(),
    () => checkClipboardData(EXPECTED_CLIPBOARD_REMOVED)
  );

  info("Hiding menu");
  menu.hide(document);

  info(
    "Click the Copy Declaration context menu item for the added declaration"
  );
  menu = await getChangesContextMenu(changesView, addDecl[0].element);
  menuItem = menu.items.find(
    item => item.id === "changes-contextmenu-copy-declaration"
  );
  await waitForClipboardPromise(
    () => menuItem.click(),
    () => checkClipboardData(EXPECTED_CLIPBOARD_ADDED)
  );
});

function checkClipboardData(expected) {
  const actual = SpecialPowers.getClipboardData("text/plain");
  return actual.trim() === expected.trim();
}
