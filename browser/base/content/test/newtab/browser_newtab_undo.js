/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that the undo dialog works as expected.
 */
function runTests() {
  // remove unpinned sites and undo it
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("5");

  yield addNewTabPageTab();
  checkGrid("5p,0,1,2,3,4,6,7,8");

  yield blockCell(4);
  yield blockCell(4);
  checkGrid("5p,0,1,2,6,7,8");

  yield undo();
  checkGrid("5p,0,1,2,4,6,7,8");

  // now remove a pinned site and undo it
  yield blockCell(0);
  checkGrid("0,1,2,4,6,7,8");

  yield undo();
  checkGrid("5p,0,1,2,4,6,7,8");

  // remove a site and restore all
  yield blockCell(1);
  checkGrid("5p,1,2,4,6,7,8");

  yield undoAll();
  checkGrid("5p,0,1,2,3,4,6,7,8");
}

function undo() {
  let cw = getContentWindow();
  let target = cw.document.getElementById("newtab-undo-button");
  EventUtils.synthesizeMouseAtCenter(target, {}, cw);
  whenPagesUpdated();
}

function undoAll() {
  let cw = getContentWindow();
  let target = cw.document.getElementById("newtab-undo-restore-button");
  EventUtils.synthesizeMouseAtCenter(target, {}, cw);
  whenPagesUpdated();
}