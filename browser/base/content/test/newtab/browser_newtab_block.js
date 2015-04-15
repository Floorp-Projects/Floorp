/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that blocking/removing sites from the grid works
 * as expected. Pinned tabs should not be moved. Gaps will be re-filled
 * if more sites are available.
 */

gDirectorySource = "data:application/json," + JSON.stringify({
  "suggested": [{
    url: "http://suggested.com/",
    imageURI: "data:image/png;base64,helloWORLD3",
    title: "title",
    type: "affiliate",
    frecent_sites: ["example0.com"]
  }]
});

function runTests() {
  let origIsTopPlacesSite = NewTabUtils.isTopPlacesSite;
  NewTabUtils.isTopPlacesSite = (site) => false;

  // we remove sites and expect the gaps to be filled as long as there still
  // are some sites available
  yield setLinks("0,1,2,3,4,5,6,7,8,9");
  setPinnedLinks("");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7,8");

  yield blockCell(4);
  checkGrid("0,1,2,3,5,6,7,8,9");

  yield blockCell(4);
  checkGrid("0,1,2,3,6,7,8,9,");

  yield blockCell(4);
  checkGrid("0,1,2,3,7,8,9,,");

  // we removed a pinned site
  yield restore();
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",1");

  yield addNewTabPageTab();
  checkGrid("0,1p,2,3,4,5,6,7,8");

  yield blockCell(1);
  checkGrid("0,2,3,4,5,6,7,8,");

  // we remove the last site on the grid (which is pinned) and expect the gap
  // to be re-filled and the new site to be unpinned
  yield restore();
  yield setLinks("0,1,2,3,4,5,6,7,8,9");
  setPinnedLinks(",,,,,,,,8");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7,8p");

  yield blockCell(8);
  checkGrid("0,1,2,3,4,5,6,7,9");

  // we remove the first site on the grid with the last one pinned. all cells
  // but the last one should shift to the left and a new site fades in
  yield restore();
  yield setLinks("0,1,2,3,4,5,6,7,8,9");
  setPinnedLinks(",,,,,,,,8");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7,8p");

  yield blockCell(0);
  checkGrid("1,2,3,4,5,6,7,9,8p");

  // Test that blocking the targeted site also removes its associated suggested tile
  NewTabUtils.isTopPlacesSite = origIsTopPlacesSite;
  yield restore();
  yield setLinks("0,1,2,3,4,5,6,7,8,9");
  yield addNewTabPageTab();
  yield customizeNewTabPage("enhanced");
  checkGrid("http://suggested.com/,0,1,2,3,4,5,6,7,8,9");

  yield blockCell(1);
  yield addNewTabPageTab();
  checkGrid("1,2,3,4,5,6,7,8,9");
}
