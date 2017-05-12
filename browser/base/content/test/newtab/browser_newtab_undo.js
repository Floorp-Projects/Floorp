/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that the undo dialog works as expected.
 */
add_task(async function() {
  // remove unpinned sites and undo it
  await setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("5");

  await addNewTabPageTab();
  await checkGrid("5p,0,1,2,3,4,6,7,8");

  await blockCell(4);
  await blockCell(4);
  await checkGrid("5p,0,1,2,6,7,8");

  await undo();
  await checkGrid("5p,0,1,2,4,6,7,8");

  // now remove a pinned site and undo it
  await blockCell(0);
  await checkGrid("0,1,2,4,6,7,8");

  await undo();
  await checkGrid("5p,0,1,2,4,6,7,8");

  // remove a site and restore all
  await blockCell(1);
  await checkGrid("5p,1,2,4,6,7,8");

  await undoAll();
  await checkGrid("5p,0,1,2,3,4,6,7,8");
});

async function undo() {
  let updatedPromise = whenPagesUpdated();
  await BrowserTestUtils.synthesizeMouseAtCenter("#newtab-undo-button", {}, gBrowser.selectedBrowser);
  await updatedPromise;
}

async function undoAll() {
  let updatedPromise = whenPagesUpdated();
  await BrowserTestUtils.synthesizeMouseAtCenter("#newtab-undo-restore-button", {}, gBrowser.selectedBrowser);
  await updatedPromise;
}
