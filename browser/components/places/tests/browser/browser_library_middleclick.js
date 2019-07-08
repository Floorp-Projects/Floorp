/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests middle-clicking items in the Library.
 */

const URIs = ["about:license", "about:mozilla"];

var gLibrary = null;
var gTests = [];

add_task(async function test_setup() {
  // Increase timeout, this test can be quite slow due to waitForFocus calls.
  requestLongerTimeout(2);

  // Temporary disable history, so we won't record pages navigation.
  await SpecialPowers.pushPrefEnv({ set: [["places.history.enabled", false]] });

  // Ensure the database is empty.
  await PlacesUtils.bookmarks.eraseEverything();

  // Open Library window.
  gLibrary = await promiseLibrary();

  registerCleanupFunction(async () => {
    // We must close "Other Bookmarks" ready for other tests.
    gLibrary.PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");
    gLibrary.PlacesOrganizer._places.selectedNode.containerOpen = false;

    await PlacesUtils.bookmarks.eraseEverything();

    // Close Library window.
    await promiseLibraryClosed(gLibrary);
  });
});

gTests.push({
  desc: "Open bookmark in a new tab.",
  URIs: ["about:buildconfig"],
  _bookmark: null,

  async setup() {
    // Add a new unsorted bookmark.
    this._bookmark = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      title: "Title",
      url: this.URIs[0],
    });

    // Select unsorted bookmarks root in the left pane.
    gLibrary.PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");
    Assert.notEqual(
      gLibrary.PlacesOrganizer._places.selectedNode,
      null,
      "We correctly have selection in the Library left pane"
    );

    // Get our bookmark in the right pane.
    var bookmarkNode = gLibrary.ContentTree.view.view.nodeForTreeIndex(0);
    Assert.equal(
      bookmarkNode.uri,
      this.URIs[0],
      "Found bookmark in the right pane"
    );
  },

  async cleanup() {
    await PlacesUtils.bookmarks.remove(this._bookmark);
  },
});

// ------------------------------------------------------------------------------
// Open a folder in tabs.
//
gTests.push({
  desc: "Open a folder in tabs.",
  URIs: ["about:buildconfig", "about:mozilla"],
  _bookmarks: null,

  async setup() {
    // Create a new folder.
    let children = this.URIs.map(url => {
      return {
        title: "Title",
        url,
      };
    });

    this._bookmarks = await PlacesUtils.bookmarks.insertTree({
      guid: PlacesUtils.bookmarks.unfiledGuid,
      children: [
        {
          title: "Folder",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          children,
        },
      ],
    });

    // Select unsorted bookmarks root in the left pane.
    gLibrary.PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");
    isnot(
      gLibrary.PlacesOrganizer._places.selectedNode,
      null,
      "We correctly have selection in the Library left pane"
    );
    // Get our bookmark in the right pane.
    var folderNode = gLibrary.ContentTree.view.view.nodeForTreeIndex(0);
    is(folderNode.title, "Folder", "Found folder in the right pane");
  },

  async cleanup() {
    await PlacesUtils.bookmarks.remove(this._bookmarks[0]);
  },
});

// ------------------------------------------------------------------------------
// Open a query in tabs.

gTests.push({
  desc: "Open a query in tabs.",
  URIs: ["about:buildconfig", "about:mozilla"],
  _bookmarks: null,
  _query: null,

  async setup() {
    let children = this.URIs.map(url => {
      return {
        title: "Title",
        url,
      };
    });

    this._bookmarks = await PlacesUtils.bookmarks.insertTree({
      guid: PlacesUtils.bookmarks.unfiledGuid,
      children: [
        {
          title: "Folder",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          children,
        },
      ],
    });

    // Create a bookmarks query containing our bookmarks.
    var hs = PlacesUtils.history;
    var options = hs.getNewQueryOptions();
    options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
    var query = hs.getNewQuery();
    // The colon included in the terms selects only about: URIs. If not included
    // we also may get pages like about.html included in the query result.
    query.searchTerms = "about:";
    var queryString = hs.queryToQueryString(query, options);
    this._query = await PlacesUtils.bookmarks.insert({
      index: 0, // it must be the first
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      title: "Query",
      url: queryString,
    });

    // Select unsorted bookmarks root in the left pane.
    gLibrary.PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");
    isnot(
      gLibrary.PlacesOrganizer._places.selectedNode,
      null,
      "We correctly have selection in the Library left pane"
    );
    // Get our bookmark in the right pane.
    var folderNode = gLibrary.ContentTree.view.view.nodeForTreeIndex(0);
    is(folderNode.title, "Query", "Found query in the right pane");
  },

  async cleanup() {
    await PlacesUtils.bookmarks.remove(this._bookmarks[0]);
    await PlacesUtils.bookmarks.remove(this._query);
  },
});

async function runTest(test) {
  info("Start of test: " + test.desc);
  // Test setup will set Library so that the bookmark to be opened is the
  // first node in the content (right pane) tree.
  await test.setup();

  // Middle click on first node in the content tree of the Library.
  gLibrary.focus();
  await SimpleTest.promiseFocus(gLibrary);

  // Now middle-click on the bookmark contained with it.
  let promiseLoaded = Promise.all(
    test.URIs.map(uri =>
      BrowserTestUtils.waitForNewTab(gBrowser, uri, false, true)
    )
  );

  mouseEventOnCell(gLibrary.ContentTree.view, 0, 0, { button: 1 });

  let tabs = await promiseLoaded;

  Assert.ok(true, "Expected tabs were loaded");

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }

  await test.cleanup();
}

add_task(async function test_all() {
  for (let test of gTests) {
    await runTest(test);
  }
});

function mouseEventOnCell(aTree, aRowIndex, aColumnIndex, aEventDetails) {
  var selection = aTree.view.selection;
  selection.select(aRowIndex);
  aTree.ensureRowIsVisible(aRowIndex);
  var column = aTree.columns[aColumnIndex];

  // get cell coordinates
  var rect = aTree.getCoordsForCellItem(aRowIndex, column, "text");

  EventUtils.synthesizeMouse(
    aTree.body,
    rect.x,
    rect.y,
    aEventDetails,
    gLibrary
  );
}
