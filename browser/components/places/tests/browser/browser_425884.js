/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  // sanity check
  ok(PlacesUtils, "checking PlacesUtils, running in chrome context?");
  ok(PlacesUIUtils, "checking PlacesUIUtils, running in chrome context?");

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

  var toolbarId = PlacesUtils.toolbarFolderId;
  var toolbarNode = PlacesUtils.getFolderContents(toolbarId).root;

  var oldCount = toolbarNode.childCount;
  var testRootId = PlacesUtils.bookmarks.createFolder(toolbarId, "test root", -1);
  is(toolbarNode.childCount, oldCount+1, "confirm test root node is a container, and is empty");
  var testRootNode = toolbarNode.getChild(toolbarNode.childCount-1);
  testRootNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  testRootNode.containerOpen = true;
  is(testRootNode.childCount, 0, "confirm test root node is a container, and is empty");

  // create folder A, fill it, validate its contents
  var folderAId = PlacesUtils.bookmarks.createFolder(testRootId, "A", -1);
  populate(folderAId);
  var folderANode = PlacesUtils.getFolderContents(folderAId).root;
  validate(folderANode);
  is(testRootNode.childCount, 1, "create test folder");

  // copy it, using the front-end helper functions
  var serializedNode = PlacesUtils.wrapNode(folderANode, PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER);
  var rawNode = PlacesUtils.unwrapNodes(serializedNode, PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER).shift();
  // confirm serialization
  ok(rawNode.type, "confirm json node");
  folderANode.containerOpen = false;

  var transaction = PlacesUIUtils.makeTransaction(rawNode,
                                                  PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER,
                                                  testRootId,
                                                  -1,
                                                  true);
  ok(transaction, "create transaction");
  PlacesUtils.transactionManager.doTransaction(transaction);
  // confirm copy
  is(testRootNode.childCount, 2, "create test folder via copy");

  // validate the copy
  var folderBNode = testRootNode.getChild(1);
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
  PlacesUtils.bookmarks.removeItem(folderAId);
}

function populate(aFolderId) {
  var folderId = PlacesUtils.bookmarks.createFolder(aFolderId, "test folder", -1);
  PlacesUtils.bookmarks.insertBookmark(folderId, PlacesUtils._uri("http://foo"), -1, "test bookmark");
  PlacesUtils.bookmarks.insertSeparator(folderId, -1);
}

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
