/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 *  Tests that Library correctly handles deletes.
 */

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const TEST_URL = "http://www.batch.delete.me/";

var gLibrary;

add_task(async function test_setup() {
  gLibrary = await promiseLibrary();

  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
    // Close Library window.
    gLibrary.close();
  });
});

add_task(async function test_create_and_remove_bookmarks() {
  let bmChildren = [];
  for (let i = 0; i < 10; i++) {
    bmChildren.push({
      title: `bm${i}`,
      url: TEST_URL,
    });
  }

  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [
      {
        title: "deleteme",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        children: bmChildren,
      },
      {
        title: "keepme",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      },
    ],
  });

  // Select and open the left pane "History" query.
  let PO = gLibrary.PlacesOrganizer;
  PO.selectLeftPaneBuiltIn("UnfiledBookmarks");
  Assert.notEqual(PO._places.selectedNode, null, "Selected unsorted bookmarks");

  let unsortedNode = PlacesUtils.asContainer(PO._places.selectedNode);
  Assert.equal(unsortedNode.childCount, 2, "Unsorted node has 2 children");
  let folderNode = unsortedNode.getChild(0);
  Assert.equal(
    folderNode.title,
    "deleteme",
    "Folder found in unsorted bookmarks"
  );

  // Check delete command is available.
  PO._places.selectNode(folderNode);
  Assert.equal(
    PO._places.selectedNode.title,
    "deleteme",
    "Folder node selected"
  );
  Assert.ok(
    PO._places.controller.isCommandEnabled("cmd_delete"),
    "Delete command is enabled"
  );
  let promiseItemRemovedNotification = PlacesTestUtils.waitForNotification(
    "bookmark-removed",
    events => events.some(event => event.guid == folderNode.bookmarkGuid)
  );

  // Press the delete key and check that the bookmark has been removed.
  gLibrary.document.getElementById("placesList").focus();
  EventUtils.synthesizeKey("VK_DELETE", {}, gLibrary);

  await promiseItemRemovedNotification;

  Assert.ok(
    !(await PlacesUtils.bookmarks.fetch({ url: TEST_URL })),
    "Bookmark has been correctly removed"
  );
  // Test live update.
  Assert.equal(unsortedNode.childCount, 1, "Unsorted node has 1 child");
  Assert.equal(PO._places.selectedNode.title, "keepme", "Folder node selected");
});

add_task(async function test_ensure_correct_selection_and_functionality() {
  let PO = gLibrary.PlacesOrganizer;
  let ContentTree = gLibrary.ContentTree;
  // Move selection forth and back.
  PO.selectLeftPaneBuiltIn("History");
  PO.selectLeftPaneBuiltIn("UnfiledBookmarks");
  // Now select the "keepme" folder in the right pane and delete it.
  ContentTree.view.selectNode(ContentTree.view.result.root.getChild(0));
  Assert.equal(
    ContentTree.view.selectedNode.title,
    "keepme",
    "Found folder in content pane"
  );

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bm",
    url: TEST_URL,
  });

  Assert.equal(
    ContentTree.view.result.root.childCount,
    2,
    "Right pane was correctly updated"
  );
});

add_task(async function test_repeated_remove_bookmark() {
  let PO = gLibrary.PlacesOrganizer;
  PO.selectLeftPaneBuiltIn("UnfiledBookmarks");

  gLibrary.document.getElementById("placesList").focus();
  let unsortedNode = PlacesUtils.asContainer(PO._places.selectedNode);
  Assert.equal(unsortedNode.childCount, 1, "Unsorted node has 1 child");
  let folderNode = unsortedNode.getChild(0);
  Assert.equal(
    folderNode.title,
    "keepme",
    "Folder found in unsorted bookmarks"
  );
  PO._places.selectNode(folderNode);

  Assert.ok(
    PO._places.controller.isCommandEnabled("cmd_delete"),
    "Delete command is enabled"
  );
  registerCleanupFunction(sinon.restore);
  let spy = sinon.spy(PO._places.controller, "remove");
  let stub = sinon
    .stub(PO._places.controller, "_removeRowsFromBookmarks")
    .resolves();
  PO._places.controller.doCommand("cmd_delete");
  PO._places.controller.doCommand("cmd_delete");
  PO._places.controller.doCommand("cmd_delete");
  Assert.equal(spy.callCount, 3, "Should have been invoked thrice");
  Assert.equal(stub.callCount, 1, "Should have been invoked once");
  // Executing another command allows delete to go through again.
  PO._places.controller.doCommand("cmd_cut");
  PO._places.controller.doCommand("cmd_delete");
  Assert.equal(spy.callCount, 4, "Should have been invoked again");
  Assert.equal(stub.callCount, 2, "Should have been invoked again");

  sinon.restore();
});

add_task(async function test_repeated_remove_separator() {
  await PlacesUtils.bookmarks.eraseEverything();

  const NUM_SEPARATORS = 2;
  for (let i = 0; i < NUM_SEPARATORS; ++i) {
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      index: 0,
    });
  }

  gLibrary.PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");
  let view = gLibrary.ContentTree.view;
  view.focus();
  let unsortedNode = PlacesUtils.asContainer(view.result.root);
  Assert.equal(2, unsortedNode.childCount, "Unsorted node has 2 children");

  for (let i = NUM_SEPARATORS - 1; i >= 0; --i) {
    let node = unsortedNode.getChild(i);
    Assert.equal(
      node.type,
      Ci.nsINavHistoryResultNode.RESULT_TYPE_SEPARATOR,
      "Separator found in unsorted bookmarks"
    );
    view.selectNode(node);
    Assert.ok(
      view.controller.isCommandEnabled("cmd_delete"),
      "Delete command is enabled"
    );
    view.controller.doCommand("cmd_delete");
    await PlacesTestUtils.waitForNotification("bookmark-removed", events =>
      events.some(({ guid }) => guid == node.bookmarkGuid)
    );
  }

  Assert.equal(unsortedNode.childCount, 0, "Unsorted node has no children");
});
