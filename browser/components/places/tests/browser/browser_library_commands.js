/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Test enabled commands in the left pane folder of the Library.
 */

const TEST_URI = NetUtil.newURI("http://www.mozilla.org/");

registerCleanupFunction(function* () {
  yield PlacesUtils.bookmarks.eraseEverything();
  yield PlacesTestUtils.clearHistory();
});

add_task(function* test_date_container() {
  let library = yield promiseLibrary();
  info("Ensure date containers under History cannot be cut but can be deleted");

  yield PlacesTestUtils.addVisits(TEST_URI);

  // Select and open the left pane "History" query.
  let PO = library.PlacesOrganizer;

  PO.selectLeftPaneQuery('History');
  isnot(PO._places.selectedNode, null, "We correctly selected History");

  // Check that both delete and cut commands are disabled, cause this is
  // a child of the left pane folder.
  ok(PO._places.controller.isCommandEnabled("cmd_copy"),
     "Copy command is enabled");
  ok(!PO._places.controller.isCommandEnabled("cmd_cut"),
     "Cut command is disabled");
  ok(!PO._places.controller.isCommandEnabled("cmd_delete"),
     "Delete command is disabled");
  let historyNode = PlacesUtils.asContainer(PO._places.selectedNode);
  historyNode.containerOpen = true;

  // Check that we have a child container. It is "Today" container.
  is(historyNode.childCount, 1, "History node has one child");
  let todayNode = historyNode.getChild(0);
  let todayNodeExpectedTitle = PlacesUtils.getString("finduri-AgeInDays-is-0");
  is(todayNode.title, todayNodeExpectedTitle,
     "History child is the expected container");

  // Select "Today" container.
  PO._places.selectNode(todayNode);
  is(PO._places.selectedNode, todayNode,
     "We correctly selected Today container");
  // Check that delete command is enabled but cut command is disabled, cause
  // this is an history item.
  ok(PO._places.controller.isCommandEnabled("cmd_copy"),
     "Copy command is enabled");
  ok(!PO._places.controller.isCommandEnabled("cmd_cut"),
     "Cut command is disabled");
  ok(PO._places.controller.isCommandEnabled("cmd_delete"),
     "Delete command is enabled");

  // Execute the delete command and check visit has been removed.
  let promiseURIRemoved = promiseHistoryNotification("onDeleteURI",
                                                     v => TEST_URI.equals(v));
  PO._places.controller.doCommand("cmd_delete");
  yield promiseURIRemoved;

  // Test live update of "History" query.
  is(historyNode.childCount, 0, "History node has no more children");

  historyNode.containerOpen = false;

  ok(!(yield promiseIsURIVisited(TEST_URI)), "Visit has been removed");

  library.close();
});

add_task(function* test_query_on_toolbar() {
  let library = yield promiseLibrary();
  info("Ensure queries can be cut or deleted");

  // Select and open the left pane "Bookmarks Toolbar" folder.
  let PO = library.PlacesOrganizer;

  PO.selectLeftPaneQuery('BookmarksToolbar');
  isnot(PO._places.selectedNode, null, "We have a valid selection");
  is(PlacesUtils.getConcreteItemId(PO._places.selectedNode),
     PlacesUtils.toolbarFolderId,
     "We have correctly selected bookmarks toolbar node.");

  // Check that both cut and delete commands are disabled, cause this is a child
  // of AllBookmarksFolderId.
  ok(PO._places.controller.isCommandEnabled("cmd_copy"),
     "Copy command is enabled");
  ok(!PO._places.controller.isCommandEnabled("cmd_cut"),
     "Cut command is disabled");
  ok(!PO._places.controller.isCommandEnabled("cmd_delete"),
     "Delete command is disabled");

  let toolbarNode = PlacesUtils.asContainer(PO._places.selectedNode);
  toolbarNode.containerOpen = true;

  // Add an History query to the toolbar.
  let query = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                   url: "place:sort=4",
                                                   title: "special_query",
                                                   parentGuid: PlacesUtils.bookmarks.toolbarGuid,
                                                   index: 0 });

  // Get first child and check it is the just inserted query.
  ok(toolbarNode.childCount > 0, "Toolbar node has children");
  let queryNode = toolbarNode.getChild(0);
  is(queryNode.title, "special_query", "Query node is correctly selected");

  // Select query node.
  PO._places.selectNode(queryNode);
  is(PO._places.selectedNode, queryNode, "We correctly selected query node");

  // Check that both cut and delete commands are enabled.
  ok(PO._places.controller.isCommandEnabled("cmd_copy"),
     "Copy command is enabled");
  ok(PO._places.controller.isCommandEnabled("cmd_cut"),
     "Cut command is enabled");
  ok(PO._places.controller.isCommandEnabled("cmd_delete"),
     "Delete command is enabled");

  // Execute the delete command and check bookmark has been removed.
  let promiseItemRemoved = promiseBookmarksNotification("onItemRemoved",
                                                        (...args) => query.guid == args[5]);
  PO._places.controller.doCommand("cmd_delete");
  yield promiseItemRemoved;

  is((yield PlacesUtils.bookmarks.fetch(query.guid)), null,
     "Query node bookmark has been correctly removed");

  toolbarNode.containerOpen = false;

  library.close();
});

add_task(function* test_search_contents() {
  let item = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                  url: "http://example.com/",
                                                  title: "example page",
                                                  parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                  index: 0 });

  let library = yield promiseLibrary();
  info("Ensure query contents can be cut or deleted");

  // Select and open the left pane "Bookmarks Toolbar" folder.
  let PO = library.PlacesOrganizer;

  PO.selectLeftPaneQuery('BookmarksToolbar');
  isnot(PO._places.selectedNode, null, "We have a valid selection");
  is(PlacesUtils.getConcreteItemId(PO._places.selectedNode),
     PlacesUtils.toolbarFolderId,
     "We have correctly selected bookmarks toolbar node.");

  let searchBox = library.document.getElementById("searchFilter");
  searchBox.value = "example";
  library.PlacesSearchBox.search(searchBox.value);

  let bookmarkNode = library.ContentTree.view.selectedNode;
  is(bookmarkNode.uri, "http://example.com/", "Found the expected bookmark");

  // Check that both cut and delete commands are enabled.
  ok(library.ContentTree.view.controller.isCommandEnabled("cmd_copy"),
     "Copy command is enabled");
  ok(library.ContentTree.view.controller.isCommandEnabled("cmd_cut"),
     "Cut command is enabled");
  ok(library.ContentTree.view.controller.isCommandEnabled("cmd_delete"),
     "Delete command is enabled");

  library.close();
});

add_task(function* test_tags() {
  let item = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                  url: "http://example.com/",
                                                  title: "example page",
                                                  parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                  index: 0 });
  PlacesUtils.tagging.tagURI(NetUtil.newURI("http://example.com/"), ["test"]);

  let library = yield promiseLibrary();
  info("Ensure query contents can be cut or deleted");

  // Select and open the left pane "Bookmarks Toolbar" folder.
  let PO = library.PlacesOrganizer;

  PO.selectLeftPaneQuery('Tags');
  let tagsNode = PO._places.selectedNode;
  isnot(tagsNode, null, "We have a valid selection");
  let tagsTitle = PlacesUtils.getString("TagsFolderTitle");
  is(tagsNode.title, tagsTitle,
     "Tags has been properly selected");

  // Check that both cut and delete commands are disabled.
  ok(PO._places.controller.isCommandEnabled("cmd_copy"),
     "Copy command is enabled");
  ok(!PO._places.controller.isCommandEnabled("cmd_cut"),
     "Cut command is disabled");
  ok(!PO._places.controller.isCommandEnabled("cmd_delete"),
     "Delete command is disabled");

  // Now select the tag.
  PlacesUtils.asContainer(tagsNode).containerOpen = true;
  let tag = tagsNode.getChild(0);
  PO._places.selectNode(tag);
  is(PO._places.selectedNode.title, "test",
     "The created tag has been properly selected");

  // Check that cut is disabled but delete is enabled.
  ok(PO._places.controller.isCommandEnabled("cmd_copy"),
     "Copy command is enabled");
  ok(!PO._places.controller.isCommandEnabled("cmd_cut"),
     "Cut command is disabled");
  ok(PO._places.controller.isCommandEnabled("cmd_delete"),
     "Delete command is enabled");

  let bookmarkNode = library.ContentTree.view.selectedNode;
  is(bookmarkNode.uri, "http://example.com/", "Found the expected bookmark");

  // Check that both cut and delete commands are enabled.
  ok(library.ContentTree.view.controller.isCommandEnabled("cmd_copy"),
     "Copy command is enabled");
  ok(!library.ContentTree.view.controller.isCommandEnabled("cmd_cut"),
     "Cut command is disabled");
  ok(library.ContentTree.view.controller.isCommandEnabled("cmd_delete"),
     "Delete command is enabled");

  tagsNode.containerOpen = false;

  library.close();
});
