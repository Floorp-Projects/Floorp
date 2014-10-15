/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /**
 * Tests middle-clicking items in the Library.
 */

const ENABLE_HISTORY_PREF = "places.history.enabled";

var gLibrary = null;
var gTests = [];
var gCurrentTest = null;

// Listener for TabOpen and tabs progress.
var gTabsListener = {
  _loadedURIs: [],
  _openTabsCount: 0,

  handleEvent: function(aEvent) {
    if (aEvent.type != "TabOpen")
      return;

    if (++this._openTabsCount == gCurrentTest.URIs.length) {
      is(gBrowser.tabs.length, gCurrentTest.URIs.length + 1,
         "We have opened " + gCurrentTest.URIs.length + " new tab(s)");
    }

    var tab = aEvent.target;
    is(tab.ownerDocument.defaultView, window,
       "Tab has been opened in current browser window");
  },

  onLocationChange: function(aBrowser, aWebProgress, aRequest, aLocationURI,
                             aFlags) {
    var spec = aLocationURI.spec;
    ok(true, spec);
    // When a new tab is opened, location is first set to "about:blank", so
    // we can ignore those calls.
    // Ignore multiple notifications for the same URI too.
    if (spec == "about:blank" || this._loadedURIs.indexOf(spec) != -1)
      return;

    ok(gCurrentTest.URIs.indexOf(spec) != -1,
       "Opened URI found in list: " + spec);

    if (gCurrentTest.URIs.indexOf(spec) != -1 )
      this._loadedURIs.push(spec);

    if (this._loadedURIs.length == gCurrentTest.URIs.length) {
      // We have correctly opened all URIs.

      // Reset arrays.
      this._loadedURIs.length = 0;

      this._openTabsCount = 0;

      executeSoon(function () {
        // Close all tabs.
        while (gBrowser.tabs.length > 1)
          gBrowser.removeCurrentTab();

        // Test finished.  This will move to the next one.
        waitForFocus(gCurrentTest.finish, gBrowser.ownerDocument.defaultView);
      });
    }
  }
}

//------------------------------------------------------------------------------
// Open bookmark in a new tab.

gTests.push({
  desc: "Open bookmark in a new tab.",
  URIs: ["about:buildconfig"],
  _itemId: -1,

  setup: function() {
    var bs = PlacesUtils.bookmarks;
    // Add a new unsorted bookmark.
    this._itemId = bs.insertBookmark(bs.unfiledBookmarksFolder,
                                     PlacesUtils._uri(this.URIs[0]),
                                     bs.DEFAULT_INDEX,
                                     "Title");
    // Select unsorted bookmarks root in the left pane.
    gLibrary.PlacesOrganizer.selectLeftPaneQuery("UnfiledBookmarks");
    isnot(gLibrary.PlacesOrganizer._places.selectedNode, null,
          "We correctly have selection in the Library left pane");
    // Get our bookmark in the right pane.
    var bookmarkNode = gLibrary.ContentTree.view.view.nodeForTreeIndex(0);
    is(bookmarkNode.uri, this.URIs[0], "Found bookmark in the right pane");
  },

  finish: function() {
    setTimeout(runNextTest, 0);
  },

  cleanup: function() {
    PlacesUtils.bookmarks.removeItem(this._itemId);
  }
});

//------------------------------------------------------------------------------
// Open a folder in tabs.

gTests.push({
  desc: "Open a folder in tabs.",
  URIs: ["about:buildconfig", "about:"],
  _folderId: -1,

  setup: function() {
    var bs = PlacesUtils.bookmarks;
    // Create a new folder.
    var folderId = bs.createFolder(bs.unfiledBookmarksFolder,
                                   "Folder",
                                   bs.DEFAULT_INDEX);
    this._folderId = folderId;

    // Add bookmarks in folder.
    this.URIs.forEach(function(aURI) {
      bs.insertBookmark(folderId,
                        PlacesUtils._uri(aURI),
                        bs.DEFAULT_INDEX,
                        "Title");
    });

    // Select unsorted bookmarks root in the left pane.
    gLibrary.PlacesOrganizer.selectLeftPaneQuery("UnfiledBookmarks");
    isnot(gLibrary.PlacesOrganizer._places.selectedNode, null,
          "We correctly have selection in the Library left pane");
    // Get our bookmark in the right pane.
    var folderNode = gLibrary.ContentTree.view.view.nodeForTreeIndex(0);
    is(folderNode.title, "Folder", "Found folder in the right pane");
  },

  finish: function() {
    setTimeout(runNextTest, 0);
  },

  cleanup: function() {
    PlacesUtils.bookmarks.removeItem(this._folderId);
  }
});

//------------------------------------------------------------------------------
// Open a query in tabs.

gTests.push({
  desc: "Open a query in tabs.",
  URIs: ["about:buildconfig", "about:"],
  _folderId: -1,
  _queryId: -1,

  setup: function() {
    var bs = PlacesUtils.bookmarks;
    // Create a new folder.
    var folderId = bs.createFolder(bs.unfiledBookmarksFolder,
                                   "Folder",
                                   bs.DEFAULT_INDEX);
    this._folderId = folderId;

    // Add bookmarks in folder.
    this.URIs.forEach(function(aURI) {
      bs.insertBookmark(folderId,
                        PlacesUtils._uri(aURI),
                        bs.DEFAULT_INDEX,
                        "Title");
    });

    // Create a bookmarks query containing our bookmarks.
    var hs = PlacesUtils.history;
    var options = hs.getNewQueryOptions();
    options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
    var query = hs.getNewQuery();
    // The colon included in the terms selects only about: URIs. If not included
    // we also may get pages like about.html included in the query result.
    query.searchTerms = "about:";
    var queryString = hs.queriesToQueryString([query], 1, options);
    this._queryId = bs.insertBookmark(bs.unfiledBookmarksFolder,
                                     PlacesUtils._uri(queryString),
                                     0, // It must be the first.
                                     "Query");

    // Select unsorted bookmarks root in the left pane.
    gLibrary.PlacesOrganizer.selectLeftPaneQuery("UnfiledBookmarks");
    isnot(gLibrary.PlacesOrganizer._places.selectedNode, null,
          "We correctly have selection in the Library left pane");
    // Get our bookmark in the right pane.
    var folderNode = gLibrary.ContentTree.view.view.nodeForTreeIndex(0);
    is(folderNode.title, "Query", "Found query in the right pane");
  },

  finish: function() {
    setTimeout(runNextTest, 0);
  },

  cleanup: function() {
    PlacesUtils.bookmarks.removeItem(this._folderId);
    PlacesUtils.bookmarks.removeItem(this._queryId);
  }
});

//------------------------------------------------------------------------------

function test() {
  waitForExplicitFinish();
  // Increase timeout, this test can be quite slow due to waitForFocus calls.
  requestLongerTimeout(2);

  // Sanity checks.
  ok(PlacesUtils, "PlacesUtils in context");
  ok(PlacesUIUtils, "PlacesUIUtils in context");

  // Add tabs listeners.
  gBrowser.tabContainer.addEventListener("TabOpen", gTabsListener, false);
  gBrowser.addTabsProgressListener(gTabsListener);

  // Temporary disable history, so we won't record pages navigation.
  gPrefService.setBoolPref(ENABLE_HISTORY_PREF, false);

  // Open Library window.
  openLibrary(function (library) {
    gLibrary = library;
    // Kick off tests.
    runNextTest();
  });
}

function runNextTest() {
  // Cleanup from previous test.
  if (gCurrentTest)
    gCurrentTest.cleanup();

  if (gTests.length > 0) {
    // Goto next test.
    gCurrentTest = gTests.shift();
    info("Start of test: " + gCurrentTest.desc);
    // Test setup will set Library so that the bookmark to be opened is the
    // first node in the content (right pane) tree.
    gCurrentTest.setup();

    // Middle click on first node in the content tree of the Library.
    gLibrary.focus();
    waitForFocus(function() {
      mouseEventOnCell(gLibrary.ContentTree.view, 0, 0, { button: 1 });
    }, gLibrary);
  }
  else {
    // No more tests.

    // Close Library window.
    gLibrary.close();

    // Remove tabs listeners.
    gBrowser.tabContainer.removeEventListener("TabOpen", gTabsListener, false);
    gBrowser.removeTabsProgressListener(gTabsListener);

    // Restore history.
    try {
      gPrefService.clearUserPref(ENABLE_HISTORY_PREF);
    } catch(ex) {}

    finish();
  }
}

function mouseEventOnCell(aTree, aRowIndex, aColumnIndex, aEventDetails) {
  var selection = aTree.view.selection;
  selection.select(aRowIndex);
  aTree.treeBoxObject.ensureRowIsVisible(aRowIndex);
  var column = aTree.columns[aColumnIndex];

  // get cell coordinates
  var rect = aTree.treeBoxObject.getCoordsForCellItem(aRowIndex, column, "text");

  EventUtils.synthesizeMouse(aTree.body, rect.x, rect.y,
                             aEventDetails, gLibrary);
}
