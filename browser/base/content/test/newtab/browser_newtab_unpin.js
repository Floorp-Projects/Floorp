/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that when a site gets unpinned it is either moved to
 * its actual place in the grid or removed in case it's not on the grid anymore.
 */
function runTests() {
  // we have a pinned link that didn't change its position since it was pinned.
  // nothing should happend when we unpin it.
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",1");

  yield addNewTabPageTab();
  checkGrid("0,1p,2,3,4,5,6,7,8");

  yield unpinCell(1);
  checkGrid("0,1,2,3,4,5,6,7,8");

  // we have a pinned link that is not anymore in the list of the most-visited
  // links. this should disappear, the remaining links adjust their positions
  // and a new link will appear at the end of the grid.
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",99");

  yield addNewTabPageTab();
  checkGrid("0,99p,1,2,3,4,5,6,7");

  yield unpinCell(1);
  checkGrid("0,1,2,3,4,5,6,7,8");

  // we have a pinned link that changed its position since it was pinned. it
  // should be moved to its new position after being unpinned.
  yield setLinks("0,1,2,3,4,5,6,7");
  setPinnedLinks(",1,,,,,,,0");

  yield addNewTabPageTab();
  checkGrid("2,1p,3,4,5,6,7,,0p");

  yield unpinCell(1);
  checkGrid("1,2,3,4,5,6,7,,0p");

  yield unpinCell(8);
  checkGrid("0,1,2,3,4,5,6,7,");

  // we have pinned link that changed its position since it was pinned. the
  // link will disappear from the grid because it's now a much lower priority
  yield setLinks("0,1,2,3,4,5,6,7,8,9");
  setPinnedLinks("9");

  yield addNewTabPageTab();
  checkGrid("9p,0,1,2,3,4,5,6,7");

  yield unpinCell(0);
  checkGrid("0,1,2,3,4,5,6,7,8");
}
