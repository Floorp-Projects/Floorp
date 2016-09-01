/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PRELOAD_PREF = "browser.newtab.preload";
const PREF_NEWTAB_COLUMNS = "browser.newtabpage.columns";
const PREF_NEWTAB_ROWS = "browser.newtabpage.rows";

function populateDirectoryTiles() {
  let directoryTiles = [];
  let i = 0;
  while (i++ < 14) {
    directoryTiles.push({
      directoryId: i,
      url: "http://example" + i + ".com/",
      enhancedImageURI: "data:image/png;base64,helloWORLD",
      title: "dirtitle" + i,
      type: "affiliate"
    });
  }
  return directoryTiles;
}

gDirectorySource = "data:application/json," + JSON.stringify({
  "directory": populateDirectoryTiles()
});


add_task(function* () {
  requestLongerTimeout(4);
  let origEnhanced = NewTabUtils.allPages.enhanced;
  let origCompareLinks = NewTabUtils.links.compareLinks;
  registerCleanupFunction(() => {
    NewTabUtils.allPages.enhanced = origEnhanced;
    NewTabUtils.links.compareLinks = origCompareLinks;
  });

  // turn off preload to ensure grid updates on every setLinks
  yield pushPrefs([PRELOAD_PREF, false]);
  // set newtab to have three columns only
  yield pushPrefs([PREF_NEWTAB_COLUMNS, 3]);
  yield pushPrefs([PREF_NEWTAB_ROWS, 5]);

  yield* addNewTabPageTab();
  yield customizeNewTabPage("enhanced"); // Toggle enhanced off

  // Testing history tiles

  // two rows of tiles should always fit on any screen
  yield setLinks("0,1,2,3,4,5");
  yield* addNewTabPageTab();

  // should do not see scrollbar since tiles fit into visible space
  yield* checkGrid("0,1,2,3,4,5");
  let scrolling = yield hasScrollbar();
  ok(!scrolling, "no scrollbar");

  // add enough tiles to cause extra two rows and observe scrollbar
  yield setLinks("0,1,2,3,4,5,6,7,8,9");
  yield* addNewTabPageTab();
  yield* checkGrid("0,1,2,3,4,5,6,7,8,9");
  scrolling = yield hasScrollbar();
  ok(scrolling, "document has scrollbar");

  // pin the last tile to make it stay at the bottom of the newtab
  yield pinCell(9);
  // block first 6 tiles, which should not remove the scroll bar
  // since the last tile is pinned in the nineth position
  for (let i = 0; i < 6; i++) {
    yield blockCell(0);
  }
  yield* addNewTabPageTab();
  yield* checkGrid("6,7,8,,,,,,,9p");
  scrolling = yield hasScrollbar();
  ok(scrolling, "document has scrollbar when tile is pinned to the last row");

  // unpin the site: this will move tile up and make scrollbar disappear
  yield unpinCell(9);
  yield* addNewTabPageTab();
  yield* checkGrid("6,7,8,9");
  scrolling = yield hasScrollbar();
  ok(!scrolling, "no scrollbar when bottom row tile is unpinned");

  // reset everything to clean slate
  NewTabUtils.restore();

  // Testing directory tiles
  yield customizeNewTabPage("enhanced"); // Toggle enhanced on

  // setup page with no history tiles to test directory only display
  yield setLinks([]);
  yield* addNewTabPageTab();
  ok(!scrolling, "no scrollbar for directory tiles");

  // introduce one history tile - it should occupy the last
  // available slot at the bottom of newtab and cause scrollbar
  yield setLinks("41");
  yield* addNewTabPageTab();
  scrolling = yield hasScrollbar();
  ok(scrolling, "adding low frecency history site causes scrollbar");

  // set PREF_NEWTAB_ROWS to 4, that should clip off the history tile
  // and remove scroll bar
  yield pushPrefs([PREF_NEWTAB_ROWS, 4]);
  yield* addNewTabPageTab();

  scrolling = yield hasScrollbar();
  ok(!scrolling, "no scrollbar if history tiles falls past max rows");

  // restore max rows and watch scrollbar re-appear
  yield pushPrefs([PREF_NEWTAB_ROWS, 5]);
  yield* addNewTabPageTab();
  scrolling = yield hasScrollbar();
  ok(scrolling, "scrollbar is back when max rows allow for bottom history tile");

  // block that history tile, and watch scrollbar disappear
  yield blockCell(14);
  yield* addNewTabPageTab();
  scrolling = yield hasScrollbar();
  ok(!scrolling, "no scrollbar after bottom history tiles is blocked");

  // Test well-populated user history - newtab has highly-frecent history sites
  // redefine compareLinks to always choose history tiles first
  NewTabUtils.links.compareLinks = function (aLink1, aLink2) {
    if (aLink1.type == aLink2.type) {
      return aLink2.frecency - aLink1.frecency ||
             aLink2.lastVisitDate - aLink1.lastVisitDate;
    }
    if (aLink2.type == "history") {
      return 1;
    }
    return -1;
  };

  // add a row of history tiles, directory tiles will be clipped off, hence no scrollbar
  yield setLinks("31,32,33");
  yield* addNewTabPageTab();
  scrolling = yield hasScrollbar();
  ok(!scrolling, "no scrollbar when directory tiles follow history tiles");

  // fill first four rows with history tiles and observer scrollbar
  yield setLinks("30,31,32,33,34,35,36,37,38,39");
  yield* addNewTabPageTab();
  scrolling = yield hasScrollbar();
  ok(scrolling, "scrollbar appears when history tiles need extra row");
});

