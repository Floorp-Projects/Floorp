/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 Deep copy of bookmark data, using the front-end codepath:

 - create test folder A
 - add a subfolder to folder A, and add items to it
 - validate folder A (sanity check)
 - copy folder A, creating new folder B, using the front-end path
 - validate folder B
 - undo copy transaction
 - validate folder B (empty)
 - redo copy transaction
 - validate folder B's contents
*/

add_task(function* () {
  // sanity check
  ok(PlacesUtils, "checking PlacesUtils, running in chrome context?");
  ok(PlacesUIUtils, "checking PlacesUIUtils, running in chrome context?");

  let toolbarId = PlacesUtils.toolbarFolderId;
  let toolbarNode = PlacesUtils.getFolderContents(toolbarId).root;

  let oldCount = toolbarNode.childCount;
  let testRoot = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "test root"
  });
  is(toolbarNode.childCount, oldCount+1, "confirm test root node is a container, and is empty");

  let testRootNode = toolbarNode.getChild(toolbarNode.childCount-1);
  testRootNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  testRootNode.containerOpen = true;
  is(testRootNode.childCount, 0, "confirm test root node is a container, and is empty");

  // create folder A, fill it, validate its contents
  let folderA = yield PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: testRoot.guid,
    title: "A"
  });

  yield populate(folderA);

  let folderAId = yield PlacesUtils.promiseItemId(folderA.guid);
  let folderANode = PlacesUtils.getFolderContents(folderAId).root;
  validate(folderANode);
  is(testRootNode.childCount, 1, "create test folder");

  // copy it, using the front-end helper functions
  let serializedNode = PlacesUtils.wrapNode(folderANode, PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER);
  let rawNode = PlacesUtils.unwrapNodes(serializedNode, PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER).shift();
  // confirm serialization
  ok(rawNode.type, "confirm json node");
  folderANode.containerOpen = false;

  let testRootId = yield PlacesUtils.promiseItemId(testRoot.guid);
  let transaction = PlacesUIUtils.makeTransaction(rawNode,
                                                  PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER,
                                                  testRootId,
                                                  -1,
                                                  true);
  ok(transaction, "create transaction");
  PlacesUtils.transactionManager.doTransaction(transaction);
  // confirm copy
  is(testRootNode.childCount, 2, "create test folder via copy");

  // validate the copy
  let folderBNode = testRootNode.getChild(1);
  validate(folderBNode);

  // undo the transaction, confirm the removal
  PlacesUtils.transactionManager.undoTransaction();
  is(testRootNode.childCount, 1, "confirm undo removed the copied folder");

  // redo the transaction
  PlacesUtils.transactionManager.redoTransaction();
  is(testRootNode.childCount, 2, "confirm redo re-copied the folder");
  folderBNode = testRootNode.getChild(1);
  validate(folderBNode);

  // Close containers, cleaning up their observers.
  testRootNode.containerOpen = false;
  toolbarNode.containerOpen = false;

  // clean up
  PlacesUtils.transactionManager.undoTransaction();
  yield PlacesUtils.bookmarks.remove(folderA.guid);
});

var populate = Task.async(function* (parentFolder) {
  let folder = yield PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: parentFolder.guid,
    title: "test folder"
  });

  yield PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: folder.guid,
    title: "test bookmark",
    url: "http://foo"
  });

  yield PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    parentGuid: folder.guid
  });
});

function validate(aNode) {
  PlacesUtils.asContainer(aNode);
  aNode.containerOpen = true;
  is(aNode.childCount, 1, "confirm child count match");
  var folderNode = aNode.getChild(0);
  is(folderNode.title, "test folder", "confirm folder title");
  PlacesUtils.asContainer(folderNode);
  folderNode.containerOpen = true;
  is(folderNode.childCount, 2, "confirm child count match");
  var bookmarkNode = folderNode.getChild(0);
  var separatorNode = folderNode.getChild(1);
  folderNode.containerOpen = false;
  aNode.containerOpen = false;
}
