/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(2);

const PREF_NEWTAB_COLUMNS = "browser.newtabpage.columns";
const PREF_NEWTAB_ROWS = "browser.newtabpage.rows";

/*
 * These tests make sure that dragging and dropping sites works as expected.
 * Sites contained in the grid need to shift around to indicate the result
 * of the drag-and-drop operation. If the grid is full and we're dragging
 * a new site into it another one gets pushed out.
 * This is a continuation of browser_newtab_drag_drop.js
 * to decrease test run time, focusing on external sites.
 */
 add_task(async function() {
  await addNewTabPageTab();

  // drag a new site onto the very first cell
  await setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",,,,,,,7,8");

  await addNewTabPageTab();
  await checkGrid("0,1,2,3,4,5,6,7p,8p");

  await simulateExternalDrop(0);
  await checkGrid("99p,0,1,2,3,4,5,7p,8p");

  // drag a new site onto the grid and make sure that pinned cells don't get
  // pushed out
  await setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",,,,,,,7,8");

  await addNewTabPageTab();
  await checkGrid("0,1,2,3,4,5,6,7p,8p");

  // force the grid to be small enough that a pinned cell could be pushed out
  await pushPrefs([PREF_NEWTAB_COLUMNS, 3]);
  await pushPrefs([PREF_NEWTAB_ROWS, 3]);
  await simulateExternalDrop(5);
  await checkGrid("0,1,2,3,4,99p,5,7p,8p");

  // drag a new site beneath a pinned cell and make sure the pinned cell is
  // not moved
  await setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",,,,,,,,8");

  await addNewTabPageTab();
  await checkGrid("0,1,2,3,4,5,6,7,8p");

  await simulateExternalDrop(5);
  await checkGrid("0,1,2,3,4,99p,5,6,8p");

  // drag a new site onto a block of pinned sites and make sure they're shifted
  // around accordingly
  await setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("0,1,2,,,,,,");

  await addNewTabPageTab();
  await checkGrid("0p,1p,2p");

  await simulateExternalDrop(1);
  await checkGrid("0p,99p,1p,2p,3,4,5,6,7");

  // force the grid to be small enough that a pinned cell could be pushed out
  // and the full list is truncated
  await setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",,,,,,,7,8");

  await addNewTabPageTab();
  await pushPrefs([PREF_NEWTAB_ROWS, 2]);
  await simulateExternalDrop(5);
  await checkGrid("0,1,2,3,4,99p");
});
