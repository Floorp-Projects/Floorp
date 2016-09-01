/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that the undo dialog works as expected.
 */
add_task(function* () {
  // remove unpinned sites and undo it
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("5");

  yield* addNewTabPageTab();
  yield* checkGrid("5p,0,1,2,3,4,6,7,8");

  yield blockCell(4);
  yield blockCell(4);
  yield* checkGrid("5p,0,1,2,6,7,8");

  yield* undo();
  yield* checkGrid("5p,0,1,2,4,6,7,8");

  // now remove a pinned site and undo it
  yield blockCell(0);
  yield* checkGrid("0,1,2,4,6,7,8");

  yield* undo();
  yield* checkGrid("5p,0,1,2,4,6,7,8");

  // remove a site and restore all
  yield blockCell(1);
  yield* checkGrid("5p,1,2,4,6,7,8");

  yield* undoAll();
  yield* checkGrid("5p,0,1,2,3,4,6,7,8");
});

function* undo() {
  let updatedPromise = whenPagesUpdated();
  yield BrowserTestUtils.synthesizeMouseAtCenter("#newtab-undo-button", {}, gBrowser.selectedBrowser);
  yield updatedPromise;
}

function* undoAll() {
  let updatedPromise = whenPagesUpdated();
  yield BrowserTestUtils.synthesizeMouseAtCenter("#newtab-undo-restore-button", {}, gBrowser.selectedBrowser);
  yield updatedPromise;
}
