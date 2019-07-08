/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Test enabled commands in the left pane folder of the Library.
 */

const TEST_URI = NetUtil.newURI("http://www.mozilla.org/");

registerCleanupFunction(async function() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});

add_task(async function test_date_container() {
  let library = await promiseLibrary();
  info("Ensure date containers under History cannot be cut but can be deleted");

  await PlacesTestUtils.addVisits(TEST_URI);

  // Select and open the left pane "History" query.
  let PO = library.PlacesOrganizer;

  PO.selectLeftPaneBuiltIn("History");
  Assert.notEqual(
    PO._places.selectedNode,
    null,
    "We correctly selected History"
  );

  // Check that both delete and cut commands are disabled, cause this is
  // a child of the left pane folder.
  Assert.ok(
    PO._places.controller.isCommandEnabled("cmd_copy"),
    "Copy command is enabled"
  );
  Assert.ok(
    !PO._places.controller.isCommandEnabled("cmd_cut"),
    "Cut command is disabled"
  );
  Assert.ok(
    !PO._places.controller.isCommandEnabled("cmd_delete"),
    "Delete command is disabled"
  );
  let historyNode = PlacesUtils.asContainer(PO._places.selectedNode);
  historyNode.containerOpen = true;

  // Check that we have a child container. It is "Today" container.
  Assert.equal(historyNode.childCount, 1, "History node has one child");
  let todayNode = historyNode.getChild(0);
  let todayNodeExpectedTitle = PlacesUtils.getString("finduri-AgeInDays-is-0");
  Assert.equal(
    todayNode.title,
    todayNodeExpectedTitle,
    "History child is the expected container"
  );

  // Select "Today" container.
  PO._places.selectNode(todayNode);
  Assert.equal(
    PO._places.selectedNode,
    todayNode,
    "We correctly selected Today container"
  );
  // Check that delete command is enabled but cut command is disabled, cause
  // this is an history item.
  Assert.ok(
    PO._places.controller.isCommandEnabled("cmd_copy"),
    "Copy command is enabled"
  );
  Assert.ok(
    !PO._places.controller.isCommandEnabled("cmd_cut"),
    "Cut command is disabled"
  );
  Assert.ok(
    PO._places.controller.isCommandEnabled("cmd_delete"),
    "Delete command is enabled"
  );

  // Execute the delete command and check visit has been removed.
  let promiseURIRemoved = PlacesTestUtils.waitForNotification(
    "onDeleteURI",
    v => TEST_URI.equals(v),
    "history"
  );
  PO._places.controller.doCommand("cmd_delete");
  await promiseURIRemoved;

  // Test live update of "History" query.
  Assert.equal(historyNode.childCount, 0, "History node has no more children");

  historyNode.containerOpen = false;

  Assert.ok(
    !(await PlacesUtils.history.hasVisits(TEST_URI)),
    "Visit has been removed"
  );

  library.close();
});

add_task(async function test_query_on_toolbar() {
  let library = await promiseLibrary();
  info("Ensure queries can be cut or deleted");

  // Select and open the left pane "Bookmarks Toolbar" folder.
  let PO = library.PlacesOrganizer;

  PO.selectLeftPaneBuiltIn("BookmarksToolbar");
  Assert.notEqual(PO._places.selectedNode, null, "We have a valid selection");
  Assert.equal(
    PlacesUtils.getConcreteItemId(PO._places.selectedNode),
    PlacesUtils.toolbarFolderId,
    "We have correctly selected bookmarks toolbar node."
  );

  // Check that both cut and delete commands are disabled, cause this is a child
  // of the All Bookmarks special query.
  Assert.ok(
    PO._places.controller.isCommandEnabled("cmd_copy"),
    "Copy command is enabled"
  );
  Assert.ok(
    !PO._places.controller.isCommandEnabled("cmd_cut"),
    "Cut command is disabled"
  );
  Assert.ok(
    !PO._places.controller.isCommandEnabled("cmd_delete"),
    "Delete command is disabled"
  );

  let toolbarNode = PlacesUtils.asContainer(PO._places.selectedNode);
  toolbarNode.containerOpen = true;

  // Add an History query to the toolbar.
  let query = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: "place:sort=4",
    title: "special_query",
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0,
  });

  // Get first child and check it is the just inserted query.
  Assert.ok(toolbarNode.childCount > 0, "Toolbar node has children");
  let queryNode = toolbarNode.getChild(0);
  Assert.equal(
    queryNode.title,
    "special_query",
    "Query node is correctly selected"
  );

  // Select query node.
  PO._places.selectNode(queryNode);
  Assert.equal(
    PO._places.selectedNode,
    queryNode,
    "We correctly selected query node"
  );

  // Check that both cut and delete commands are enabled.
  Assert.ok(
    PO._places.controller.isCommandEnabled("cmd_copy"),
    "Copy command is enabled"
  );
  Assert.ok(
    PO._places.controller.isCommandEnabled("cmd_cut"),
    "Cut command is enabled"
  );
  Assert.ok(
    PO._places.controller.isCommandEnabled("cmd_delete"),
    "Delete command is enabled"
  );

  // Execute the delete command and check bookmark has been removed.
  let promiseItemRemoved = PlacesTestUtils.waitForNotification(
    "onItemRemoved",
    (...args) => query.guid == args[5]
  );
  PO._places.controller.doCommand("cmd_delete");
  await promiseItemRemoved;

  Assert.equal(
    await PlacesUtils.bookmarks.fetch(query.guid),
    null,
    "Query node bookmark has been correctly removed"
  );

  toolbarNode.containerOpen = false;

  library.close();
});

add_task(async function test_search_contents() {
  await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: "http://example.com/",
    title: "example page",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0,
  });

  let library = await promiseLibrary();
  info("Ensure query contents can be cut or deleted");

  // Select and open the left pane "Bookmarks Toolbar" folder.
  let PO = library.PlacesOrganizer;

  PO.selectLeftPaneBuiltIn("BookmarksToolbar");
  Assert.notEqual(PO._places.selectedNode, null, "We have a valid selection");
  Assert.equal(
    PlacesUtils.getConcreteItemId(PO._places.selectedNode),
    PlacesUtils.toolbarFolderId,
    "We have correctly selected bookmarks toolbar node."
  );

  let searchBox = library.document.getElementById("searchFilter");
  searchBox.value = "example";
  library.PlacesSearchBox.search(searchBox.value);

  let bookmarkNode = library.ContentTree.view.selectedNode;
  Assert.equal(
    bookmarkNode.uri,
    "http://example.com/",
    "Found the expected bookmark"
  );

  // Check that both cut and delete commands are enabled.
  Assert.ok(
    library.ContentTree.view.controller.isCommandEnabled("cmd_copy"),
    "Copy command is enabled"
  );
  Assert.ok(
    library.ContentTree.view.controller.isCommandEnabled("cmd_cut"),
    "Cut command is enabled"
  );
  Assert.ok(
    library.ContentTree.view.controller.isCommandEnabled("cmd_delete"),
    "Delete command is enabled"
  );

  library.close();
});

add_task(async function test_tags() {
  await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: "http://example.com/",
    title: "example page",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0,
  });
  PlacesUtils.tagging.tagURI(NetUtil.newURI("http://example.com/"), ["test"]);

  let library = await promiseLibrary();
  info("Ensure query contents can be cut or deleted");

  // Select and open the left pane "Bookmarks Toolbar" folder.
  let PO = library.PlacesOrganizer;

  PO.selectLeftPaneBuiltIn("Tags");
  let tagsNode = PO._places.selectedNode;
  Assert.notEqual(tagsNode, null, "We have a valid selection");
  let tagsTitle = PlacesUtils.getString("TagsFolderTitle");
  Assert.equal(tagsNode.title, tagsTitle, "Tags has been properly selected");

  // Check that both cut and delete commands are disabled.
  Assert.ok(
    PO._places.controller.isCommandEnabled("cmd_copy"),
    "Copy command is enabled"
  );
  Assert.ok(
    !PO._places.controller.isCommandEnabled("cmd_cut"),
    "Cut command is disabled"
  );
  Assert.ok(
    !PO._places.controller.isCommandEnabled("cmd_delete"),
    "Delete command is disabled"
  );

  // Now select the tag.
  PlacesUtils.asContainer(tagsNode).containerOpen = true;
  let tag = tagsNode.getChild(0);
  PO._places.selectNode(tag);
  Assert.equal(
    PO._places.selectedNode.title,
    "test",
    "The created tag has been properly selected"
  );

  // Check that cut is disabled but delete is enabled.
  Assert.ok(
    PO._places.controller.isCommandEnabled("cmd_copy"),
    "Copy command is enabled"
  );
  Assert.ok(
    !PO._places.controller.isCommandEnabled("cmd_cut"),
    "Cut command is disabled"
  );
  Assert.ok(
    PO._places.controller.isCommandEnabled("cmd_delete"),
    "Delete command is enabled"
  );

  let bookmarkNode = library.ContentTree.view.selectedNode;
  Assert.equal(
    bookmarkNode.uri,
    "http://example.com/",
    "Found the expected bookmark"
  );

  // Check that both cut and delete commands are enabled.
  Assert.ok(
    library.ContentTree.view.controller.isCommandEnabled("cmd_copy"),
    "Copy command is enabled"
  );
  Assert.ok(
    !library.ContentTree.view.controller.isCommandEnabled("cmd_cut"),
    "Cut command is disabled"
  );
  Assert.ok(
    library.ContentTree.view.controller.isCommandEnabled("cmd_delete"),
    "Delete command is enabled"
  );

  tagsNode.containerOpen = false;

  library.close();
});
