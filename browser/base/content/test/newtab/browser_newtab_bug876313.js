/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This test makes sure that the changes made by unpinning
 * a site are actually written to NewTabUtils' storage.
 */
add_task(function* () {
  // Second cell is pinned with page #99.
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",99");

  yield* addNewTabPageTab();
  yield* checkGrid("0,99p,1,2,3,4,5,6,7");

  // Unpin the second cell's site.
  yield unpinCell(1);
  yield* checkGrid("0,1,2,3,4,5,6,7,8");

  // Clear the pinned cache to force NewTabUtils to read the pref again.
  NewTabUtils.pinnedLinks.resetCache();
  NewTabUtils.allPages.update();
  yield* checkGrid("0,1,2,3,4,5,6,7,8");
});
