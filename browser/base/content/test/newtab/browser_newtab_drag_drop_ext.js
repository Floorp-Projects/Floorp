/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that dragging and dropping sites works as expected.
 * Sites contained in the grid need to shift around to indicate the result
 * of the drag-and-drop operation. If the grid is full and we're dragging
 * a new site into it another one gets pushed out.
 * This is a continuation of browser_newtab_drag_drop.js
 * to decrease test run time, focusing on external sites.
 */
function runTests() {
  // drag a new site onto the very first cell
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",,,,,,,7,8");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7p,8p");

  yield simulateExternalDrop(0);
  checkGrid("99p,0,1,2,3,4,5,7p,8p");

  // drag a new site onto the grid and make sure that pinned cells don't get
  // pushed out
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",,,,,,,7,8");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7p,8p");

  yield simulateExternalDrop(7);
  checkGrid("0,1,2,3,4,5,7p,99p,8p");

  // drag a new site beneath a pinned cell and make sure the pinned cell is
  // not moved
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",,,,,,,,8");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7,8p");

  yield simulateExternalDrop(7);
  checkGrid("0,1,2,3,4,5,6,99p,8p");

  // drag a new site onto a block of pinned sites and make sure they're shifted
  // around accordingly
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("0,1,2,,,,,,");

  yield addNewTabPageTab();
  checkGrid("0p,1p,2p");

  yield simulateExternalDrop(1);
  checkGrid("0p,99p,1p,2p,3,4,5,6,7");
}