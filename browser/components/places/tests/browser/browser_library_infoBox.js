/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Test appropriate visibility of infoBoxExpanderWrapper and
 *  additionalInfoFields in infoBox section of library
 */

const TEST_URI = "http://www.mozilla.org/";

var gTests = [];
var gLibrary;

// ------------------------------------------------------------------------------

gTests.push({
  desc: "Bug 430148 - Remove or hide the more/less button in details pane...",
  run: function() {
    var PO = gLibrary.PlacesOrganizer;
    let ContentTree = gLibrary.ContentTree;
    var infoBoxExpanderWrapper = getAndCheckElmtById("infoBoxExpanderWrapper");

    function addVisitsCallback() {
      // open all bookmarks node
      PO.selectLeftPaneQuery("AllBookmarks");
      isnot(PO._places.selectedNode, null,
            "Correctly selected all bookmarks node.");
      checkInfoBoxSelected(PO);
      ok(infoBoxExpanderWrapper.hidden,
         "Expander button is hidden for all bookmarks node.");
      checkAddInfoFieldsCollapsed(PO);

      // open history node
      PO.selectLeftPaneQuery("History");
      isnot(PO._places.selectedNode, null, "Correctly selected history node.");
      checkInfoBoxSelected(PO);
      ok(infoBoxExpanderWrapper.hidden,
         "Expander button is hidden for history node.");
      checkAddInfoFieldsCollapsed(PO);

      // open history child node
      var historyNode = PO._places.selectedNode.
                        QueryInterface(Ci.nsINavHistoryContainerResultNode);
      historyNode.containerOpen = true;
      var childNode = historyNode.getChild(0);
      isnot(childNode, null, "History node first child is not null.");
      PO._places.selectNode(childNode);
      checkInfoBoxSelected(PO);
      ok(infoBoxExpanderWrapper.hidden,
         "Expander button is hidden for history child node.");
      checkAddInfoFieldsCollapsed(PO);

      // open history item
      var view = ContentTree.view.view;
      ok(view.rowCount > 0, "History item exists.");
      view.selection.select(0);
      ok(infoBoxExpanderWrapper.hidden,
         "Expander button is hidden for history item.");
      checkAddInfoFieldsCollapsed(PO);

      historyNode.containerOpen = false;

      // open bookmarks menu node
      PO.selectLeftPaneQuery("BookmarksMenu");
      isnot(PO._places.selectedNode, null,
            "Correctly selected bookmarks menu node.");
      checkInfoBoxSelected(PO);
      ok(infoBoxExpanderWrapper.hidden,
         "Expander button is hidden for bookmarks menu node.");
      checkAddInfoFieldsCollapsed(PO);

      // open recently bookmarked node
      PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarksMenuFolderId,
                                           NetUtil.newURI("place:folder=BOOKMARKS_MENU" +
                                                          "&folder=UNFILED_BOOKMARKS" +
                                                          "&folder=TOOLBAR" +
                                                          "&queryType=" + Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS +
                                                          "&sort=" + Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_DESCENDING +
                                                          "&maxResults=10" +
                                                          "&excludeQueries=1"),
                                           0, "Recent Bookmarks");
      PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarksMenuFolderId,
                                           NetUtil.newURI("http://mozilla.org/"),
                                           1, "Mozilla");
      var menuNode = PO._places.selectedNode.
                     QueryInterface(Ci.nsINavHistoryContainerResultNode);
      menuNode.containerOpen = true;
      childNode = menuNode.getChild(0);
      isnot(childNode, null, "Bookmarks menu child node exists.");
      is(childNode.title, "Recent Bookmarks",
         "Correctly selected recently bookmarked node.");
      PO._places.selectNode(childNode);
      checkInfoBoxSelected(PO);
      ok(!infoBoxExpanderWrapper.hidden,
         "Expander button is not hidden for recently bookmarked node.");
      checkAddInfoFieldsNotCollapsed(PO);

      // open first bookmark
      view = ContentTree.view.view;
      ok(view.rowCount > 0, "Bookmark item exists.");
      view.selection.select(0);
      checkInfoBoxSelected(PO);
      ok(!infoBoxExpanderWrapper.hidden,
         "Expander button is not hidden for bookmark item.");
      checkAddInfoFieldsNotCollapsed(PO);
      checkAddInfoFields(PO, "bookmark item");

      menuNode.containerOpen = false;

      PlacesTestUtils.clearHistory().then(nextTest);
    }
    // add a visit to browser history
    PlacesTestUtils.addVisits(
      { uri: PlacesUtils._uri(TEST_URI), visitDate: Date.now() * 1000,
        transition: PlacesUtils.history.TRANSITION_TYPED }
      ).then(addVisitsCallback);
  }
});

function checkInfoBoxSelected(PO) {
  is(getAndCheckElmtById("detailsDeck").selectedIndex, 1,
     "Selected element in detailsDeck is infoBox.");
}

function checkAddInfoFieldsCollapsed(PO) {
  PO._additionalInfoFields.forEach(function (id) {
    ok(getAndCheckElmtById(id).collapsed,
       "Additional info field correctly collapsed: #" + id);
  });
}

function checkAddInfoFieldsNotCollapsed(PO) {
  ok(PO._additionalInfoFields.some(function (id) {
      return !getAndCheckElmtById(id).collapsed;
     }), "Some additional info field correctly not collapsed");
}

function checkAddInfoFields(PO, nodeName) {
  ok(true, "Checking additional info fields visibiity for node: " + nodeName);
  var expanderButton = getAndCheckElmtById("infoBoxExpander");

  // make sure additional fields are hidden by default
  PO._additionalInfoFields.forEach(function (id) {
    ok(getAndCheckElmtById(id).hidden,
       "Additional info field correctly hidden by default: #" + id);
  });

  // toggle fields and make sure they are hidden/unhidden as expected
  expanderButton.click();
  PO._additionalInfoFields.forEach(function (id) {
    ok(!getAndCheckElmtById(id).hidden,
       "Additional info field correctly unhidden after toggle: #" + id);
  });
  expanderButton.click();
  PO._additionalInfoFields.forEach(function (id) {
    ok(getAndCheckElmtById(id).hidden,
       "Additional info field correctly hidden after toggle: #" + id);
  });
}

function getAndCheckElmtById(id) {
  var elmt = gLibrary.document.getElementById(id);
  isnot(elmt, null, "Correctly got element: #" + id);
  return elmt;
}

// ------------------------------------------------------------------------------

function nextTest() {
  if (gTests.length) {
    var testCase = gTests.shift();
    ok(true, "TEST: " + testCase.desc);
    dump("TEST: " + testCase.desc + "\n");
    testCase.run();
  }
  else {
    // Close Library window.
    gLibrary.close();
    // No need to cleanup anything, we have a correct left pane now.
    finish();
  }
}

function test() {
  waitForExplicitFinish();
  // Sanity checks.
  ok(PlacesUtils, "PlacesUtils is running in chrome context");
  ok(PlacesUIUtils, "PlacesUIUtils is running in chrome context");

  // Open Library.
  openLibrary(function (library) {
    gLibrary = library;
    gLibrary.PlacesOrganizer._places.focus();
    nextTest(gLibrary);
  });
}
