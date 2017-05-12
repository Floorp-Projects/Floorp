/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that when a site gets unpinned it is either moved to
 * its actual place in the grid or removed in case it's not on the grid anymore.
 */
add_task(async function() {
  // we have a pinned link that didn't change its position since it was pinned.
  // nothing should happen when we unpin it.
  await setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",1");

  await addNewTabPageTab();
  await checkGrid("0,1p,2,3,4,5,6,7,8");

  await unpinCell(1);
  await checkGrid("0,1,2,3,4,5,6,7,8");

  // we have a pinned link that is not anymore in the list of the most-visited
  // links. this should disappear, the remaining links adjust their positions
  // and a new link will appear at the end of the grid.
  await setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",99");

  await addNewTabPageTab();
  await checkGrid("0,99p,1,2,3,4,5,6,7");

  await unpinCell(1);
  await checkGrid("0,1,2,3,4,5,6,7,8");

  // we have a pinned link that changed its position since it was pinned. it
  // should be moved to its new position after being unpinned.
  await setLinks("0,1,2,3,4,5,6,7");
  setPinnedLinks(",1,,,,,,,0");

  await addNewTabPageTab();
  await checkGrid("2,1p,3,4,5,6,7,,0p");

  await unpinCell(1);
  await checkGrid("1,2,3,4,5,6,7,,0p");

  await unpinCell(8);
  await checkGrid("0,1,2,3,4,5,6,7,");

  // we have pinned link that changed its position since it was pinned. the
  // link will disappear from the grid because it's now a much lower priority
  await setLinks("0,1,2,3,4,5,6,7,8,9");
  setPinnedLinks("9");

  await addNewTabPageTab();
  await checkGrid("9p,0,1,2,3,4,5,6,7");

  await unpinCell(0);
  await checkGrid("0,1,2,3,4,5,6,7,8");
});
