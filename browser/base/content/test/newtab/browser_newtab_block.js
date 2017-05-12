/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(2);

/*
 * These tests make sure that blocking/removing sites from the grid works
 * as expected. Pinned tabs should not be moved. Gaps will be re-filled
 * if more sites are available.
 */

add_task(async function() {
  // we remove sites and expect the gaps to be filled as long as there still
  // are some sites available
  await setLinks("0,1,2,3,4,5,6,7,8,9");
  setPinnedLinks("");

  await addNewTabPageTab();
  await customizeNewTabPage("enhanced"); // Toggle enhanced off
  await checkGrid("0,1,2,3,4,5,6,7,8");

  await blockCell(4);
  await checkGrid("0,1,2,3,5,6,7,8,9");

  await blockCell(4);
  await checkGrid("0,1,2,3,6,7,8,9,");

  await blockCell(4);
  await checkGrid("0,1,2,3,7,8,9,,");

  // we removed a pinned site
  await restore();
  await setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",1");

  await addNewTabPageTab();
  await checkGrid("0,1p,2,3,4,5,6,7,8");

  await blockCell(1);
  await checkGrid("0,2,3,4,5,6,7,8,");

  // we remove the last site on the grid (which is pinned) and expect the gap
  // to be re-filled and the new site to be unpinned
  await restore();
  await setLinks("0,1,2,3,4,5,6,7,8,9");
  setPinnedLinks(",,,,,,,,8");

  await addNewTabPageTab();
  await checkGrid("0,1,2,3,4,5,6,7,8p");

  await blockCell(8);
  await checkGrid("0,1,2,3,4,5,6,7,9");

  // we remove the first site on the grid with the last one pinned. all cells
  // but the last one should shift to the left and a new site fades in
  await restore();
  await setLinks("0,1,2,3,4,5,6,7,8,9");
  setPinnedLinks(",,,,,,,,8");

  await addNewTabPageTab();
  await checkGrid("0,1,2,3,4,5,6,7,8p");

  await blockCell(0);
  await checkGrid("1,2,3,4,5,6,7,9,8p");
});
