/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(2);

const PREF_NEWTAB_COLUMNS = "browser.newtabpage.columns";

/*
 * These tests make sure that dragging and dropping sites works as expected.
 * Sites contained in the grid need to shift around to indicate the result
 * of the drag-and-drop operation. If the grid is full and we're dragging
 * a new site into it another one gets pushed out.
 * This is a continuation of browser_newtab_drag_drop.js
 * to decrease test run time, focusing on external sites.
 */
 add_task(function* () {
  yield* addNewTabPageTab();

  // drag a new site onto the very first cell
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",,,,,,,7,8");

  yield* addNewTabPageTab();
  yield* checkGrid("0,1,2,3,4,5,6,7p,8p");

  yield* simulateExternalDrop(0);
  yield* checkGrid("99p,0,1,2,3,4,5,7p,8p");

  // drag a new site onto the grid and make sure that pinned cells don't get
  // pushed out
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",,,,,,,7,8");

  yield* addNewTabPageTab();
  yield* checkGrid("0,1,2,3,4,5,6,7p,8p");

  // force the grid to be small enough that a pinned cell could be pushed out
  yield pushPrefs([PREF_NEWTAB_COLUMNS, 3]);
  yield* simulateExternalDrop(5);
  yield* checkGrid("0,1,2,3,4,99p,5,7p,8p");

  // drag a new site beneath a pinned cell and make sure the pinned cell is
  // not moved
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",,,,,,,,8");

  yield* addNewTabPageTab();
  yield* checkGrid("0,1,2,3,4,5,6,7,8p");

  yield* simulateExternalDrop(5);
  yield* checkGrid("0,1,2,3,4,99p,5,6,8p");

  // drag a new site onto a block of pinned sites and make sure they're shifted
  // around accordingly
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("0,1,2,,,,,,");

  yield* addNewTabPageTab();
  yield* checkGrid("0p,1p,2p");

  yield* simulateExternalDrop(1);
  yield* checkGrid("0p,99p,1p,2p,3,4,5,6,7");
});
