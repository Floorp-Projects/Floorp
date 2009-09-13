/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places test code.
 *
 * The Initial Developer of the Original Code is Mozilla Corp.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

 /**
 * Tests middle-clicking items in the Library.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;

const DISABLE_HISTORY_PREF = "browser.history_expire_days";

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
      is(gBrowser.mTabs.length, gCurrentTest.URIs.length + 1,
         "We have opened " + gCurrentTest.URIs.length + " new tab(s)");
    }

    var tab = aEvent.target;
    is(tab.ownerDocument.defaultView, window,
       "Tab has been opened in current browser window");
  },

  onLocationChange: function(aBrowser, aWebProgress, aRequest, aLocationURI) {
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

    var fm = Components.classes["@mozilla.org/focus-manager;1"].
               getService(Components.interfaces.nsIFocusManager);
    is(fm.activeWindow, gBrowser.ownerDocument.defaultView, "window made active");

    if (this._loadedURIs.length == gCurrentTest.URIs.length) {
      // We have correctly opened all URIs.

      // Reset arrays.
      this._loadedURIs.length = 0;
      // Close all tabs.
      while (gBrowser.mTabs.length > 1)
        gBrowser.removeCurrentTab();
      this._openTabsCount = 0;

      // Test finished.  This will move to the next one.
      gCurrentTest.finish();
    }
  },

  onProgressChange: function(aBrowser, aWebProgress, aRequest,
                             aCurSelfProgress, aMaxSelfProgress,
                             aCurTotalProgress, aMaxTotalProgress) {
  },
  onStateChange: function(aBrowser, aWebProgress, aRequest,
                          aStateFlags, aStatus) {
  },  
  onStatusChange: function(aBrowser, aWebProgress, aRequest,
                           aStatus, aMessage) {
  },
  onSecurityChange: function(aBrowser, aWebProgress, aRequest, aState) {
  },
  noLinkIconAvailable: function(aBrowser) {
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
    var bookmarkNode = gLibrary.PlacesOrganizer._content.view.nodeForTreeIndex(0);
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
    var folderNode = gLibrary.PlacesOrganizer._content.view.nodeForTreeIndex(0);
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
    query.searchTerms = "about";
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
    var folderNode = gLibrary.PlacesOrganizer._content.view.nodeForTreeIndex(0);
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

  // Sanity checks.
  ok(PlacesUtils, "PlacesUtils in context");
  ok(PlacesUIUtils, "PlacesUIUtils in context");

  // Add tabs listeners.
  gBrowser.tabContainer.addEventListener("TabOpen", gTabsListener, false);
  gBrowser.addTabsProgressListener(gTabsListener);

  // Temporary disable history, so we won't record pages navigation.
  gPrefService.setIntPref(DISABLE_HISTORY_PREF, 0);

  // Window watcher for Library window.
  var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
           getService(Ci.nsIWindowWatcher);
  var windowObserver = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic === "domwindowopened") {
        ww.unregisterNotification(this);
        gLibrary = aSubject.QueryInterface(Ci.nsIDOMWindow);
        gLibrary.addEventListener("load", function onLoad(event) {
          gLibrary.removeEventListener("load", onLoad, false);
          // Kick off tests.
          setTimeout(runNextTest, 0);
        }, false);
      }
    }
  };

  // Open Library window.
  ww.registerNotification(windowObserver);
  ww.openWindow(null,
                "chrome://browser/content/places/places.xul",
                "",
                "chrome,toolbar=yes,dialog=no,resizable",
                null); 
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
    gLibrary.PlacesOrganizer._content.focus();
    mouseEventOnCell(gLibrary.PlacesOrganizer._content, 0, 0, { button: 1 });
  }
  else {
    // No more tests.

    // Close Library window.
    gLibrary.close();

    // Remove tabs listeners.
    gBrowser.tabContainer.removeEventListener("TabOpen", gTabsListener, false);
    gBrowser.removeTabsProgressListener(gTabsListener);

    // Restore history.
    gPrefService.clearUserPref(DISABLE_HISTORY_PREF);

    finish();
  }
}

function mouseEventOnCell(aTree, aRowIndex, aColumnIndex, aEventDetails) {
  var selection = aTree.view.selection;
  selection.select(aRowIndex);
  aTree.treeBoxObject.ensureRowIsVisible(aRowIndex);
  var column = aTree.columns[aColumnIndex];

  // get cell coordinates
  var x = {}, y = {}, width = {}, height = {};
  aTree.treeBoxObject.getCoordsForCellItem(aRowIndex, column, "text",
                                           x, y, width, height);

  EventUtils.synthesizeMouse(aTree.body, x.value, y.value,
                             aEventDetails, gLibrary);
}
