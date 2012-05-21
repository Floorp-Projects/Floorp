/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Test enabled commands in the left pane folder of the Library.
 */

const TEST_URI = "http://www.mozilla.org/";

var gTests = [];
var gLibrary;

//------------------------------------------------------------------------------

gTests.push({
  desc: "Bug 489351 - Date containers under History in Library cannot be deleted/cut",
  run: function() {
    var bhist = PlacesUtils.history.QueryInterface(Ci.nsIBrowserHistory);
    // Add a visit.
    PlacesUtils.history.addVisit(PlacesUtils._uri(TEST_URI), Date.now() * 1000,
                                 null, PlacesUtils.history.TRANSITION_TYPED,
                                 false, 0);
    ok(bhist.isVisited(PlacesUtils._uri(TEST_URI)), "Visit has been added");

    // Select and open the left pane "History" query.
    var PO = gLibrary.PlacesOrganizer;
    PO.selectLeftPaneQuery('History');
    isnot(PO._places.selectedNode, null, "We correctly selected History");

    // Check that both delete and cut commands are disabled.
    ok(!PO._places.controller.isCommandEnabled("cmd_cut"),
       "Cut command is disabled");
    ok(!PO._places.controller.isCommandEnabled("cmd_delete"),
       "Delete command is disabled");
    var historyNode = PO._places.selectedNode
                        .QueryInterface(Ci.nsINavHistoryContainerResultNode);
    historyNode.containerOpen = true;

    // Check that we have a child container. It is "Today" container.
    is(historyNode.childCount, 1, "History node has one child");
    var todayNode = historyNode.getChild(0);
    var todayNodeExpectedTitle = PlacesUtils.getString("finduri-AgeInDays-is-0");
    is(todayNode.title, todayNodeExpectedTitle,
       "History child is the expected container");

    // Select "Today" container.
    PO._places.selectNode(todayNode);
    is(PO._places.selectedNode, todayNode,
       "We correctly selected Today container");
    // Check that delete command is enabled but cut command is disabled.
    ok(!PO._places.controller.isCommandEnabled("cmd_cut"),
       "Cut command is disabled");
    ok(PO._places.controller.isCommandEnabled("cmd_delete"),
       "Delete command is enabled");

    // Execute the delete command and check visit has been removed.
    PO._places.controller.doCommand("cmd_delete");
    ok(!bhist.isVisited(PlacesUtils._uri(TEST_URI)), "Visit has been removed");

    // Test live update of "History" query.
    is(historyNode.childCount, 0, "History node has no more children");

    historyNode.containerOpen = false;
    nextTest();
  }
});

//------------------------------------------------------------------------------

gTests.push({
  desc: "Bug 490156 - Can't delete smart bookmark containers",
  run: function() {
    // Select and open the left pane "Bookmarks Toolbar" folder.
    var PO = gLibrary.PlacesOrganizer;
    PO.selectLeftPaneQuery('BookmarksToolbar');
    isnot(PO._places.selectedNode, null, "We have a valid selection");
    is(PlacesUtils.getConcreteItemId(PO._places.selectedNode),
       PlacesUtils.toolbarFolderId,
       "We have correctly selected bookmarks toolbar node.");

    // Check that both cut and delete commands are disabled.
    ok(!PO._places.controller.isCommandEnabled("cmd_cut"),
       "Cut command is disabled");
    ok(!PO._places.controller.isCommandEnabled("cmd_delete"),
       "Delete command is disabled");

    var toolbarNode = PO._places.selectedNode
                        .QueryInterface(Ci.nsINavHistoryContainerResultNode);
    toolbarNode.containerOpen = true;

    // Add an History query to the toolbar.
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.toolbarFolderId,
                                         PlacesUtils._uri("place:sort=4"),
                                         0, // Insert at start.
                                         "special_query");
    // Get first child and check it is the "Most Visited" smart bookmark.
    ok(toolbarNode.childCount > 0, "Toolbar node has children");
    var queryNode = toolbarNode.getChild(0);
    is(queryNode.title, "special_query", "Query node is correctly selected");

    // Select query node.
    PO._places.selectNode(queryNode);
    is(PO._places.selectedNode, queryNode, "We correctly selected query node");

    // Check that both cut and delete commands are enabled.
    ok(PO._places.controller.isCommandEnabled("cmd_cut"),
       "Cut command is enabled");
    ok(PO._places.controller.isCommandEnabled("cmd_delete"),
       "Delete command is enabled");

    // Execute the delete command and check bookmark has been removed.
    PO._places.controller.doCommand("cmd_delete");
    try {
      PlacesUtils.bookmarks.getFolderIdForItem(queryNode.itemId);  
      ok(false, "Unable to remove query node bookmark");
    } catch(ex) {
      ok(true, "Query node bookmark has been correctly removed");
    }

    toolbarNode.containerOpen = false;
    nextTest();
  }
});

//------------------------------------------------------------------------------

function nextTest() {
  if (gTests.length) {
    var test = gTests.shift();
    info("Start of test: " + test.desc);
    test.run();
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
  gLibrary = openLibrary(nextTest);
}
