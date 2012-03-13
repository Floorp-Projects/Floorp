/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that all changes that are made to a specific
 * 'New Tab Page' are synchronized with all other open 'New Tab Pages'
 * automatically. All about:newtab pages should always be in the same
 * state.
 */
function runTests() {
  // Disabled until bug 716543 is fixed.
  return;

  setLinks("0,1,2,3,4,5,6,7,8,9");
  setPinnedLinks(",1");

  yield addNewTabPageTab();
  checkGrid("0,1p,2,3,4,5,6,7,8");

  let resetButton = cw.document.getElementById("toolbar-button-reset");
  ok(!resetButton.hasAttribute("modified"), "page is not modified");

  let oldCw = cw;
  let oldResetButton = resetButton;

  // create the new tab page
  yield addNewTabPageTab();
  checkGrid("0,1p,2,3,4,5,6,7,8");

  resetButton = cw.document.getElementById("toolbar-button-reset");
  ok(!resetButton.hasAttribute("modified"), "page is not modified");

  // unpin a cell
  yield unpinCell(cells[1]);
  checkGrid("0,1,2,3,4,5,6,7,8");
  checkGrid("0,1,2,3,4,5,6,7,8", oldCw.gGrid.sites);

  // remove a cell
  yield blockCell(cells[1]);
  checkGrid("0,2,3,4,5,6,7,8,9");
  checkGrid("0,2,3,4,5,6,7,8,9", oldCw.gGrid.sites);
  ok(resetButton.hasAttribute("modified"), "page is modified");
  ok(oldResetButton.hasAttribute("modified"), "page is modified");

  // insert a new cell by dragging
  yield simulateDrop(cells[1]);
  checkGrid("0,99p,2,3,4,5,6,7,8");
  checkGrid("0,99p,2,3,4,5,6,7,8", oldCw.gGrid.sites);

  // drag a cell around
  yield simulateDrop(cells[1], cells[2]);
  checkGrid("0,2p,99p,3,4,5,6,7,8");
  checkGrid("0,2p,99p,3,4,5,6,7,8", oldCw.gGrid.sites);

  // reset the new tab page
  yield cw.gToolbar.reset(TestRunner.next);
  checkGrid("0,1,2,3,4,5,6,7,8");
  checkGrid("0,1,2,3,4,5,6,7,8", oldCw.gGrid.sites);
  ok(!resetButton.hasAttribute("modified"), "page is not modified");
  ok(!oldResetButton.hasAttribute("modified"), "page is not modified");
}
