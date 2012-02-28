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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dietrich Ayala <dietrich@mozilla.com>
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

function test() {
  // sanity check
  ok(PlacesUtils, "checking PlacesUtils, running in chrome context?");
  ok(PlacesUIUtils, "checking PlacesUIUtils, running in chrome context?");

  /*
  - create, a test folder, add bookmark, separator to it
  - fetch guids for all
  - copy the folder
  - test that guids are all different
  - undo copy
  - redo copy
  - test that guids for the copy stay the same
  */

  var toolbarId = PlacesUtils.toolbarFolderId;
  var toolbarNode = PlacesUtils.getFolderContents(toolbarId).root;

  var oldCount = toolbarNode.childCount;
  var testRootId = PlacesUtils.bookmarks.createFolder(toolbarId, "test root", -1);
  is(toolbarNode.childCount, oldCount+1, "confirm test root node is a container, and is empty");
  var testRootNode = toolbarNode.getChild(toolbarNode.childCount-1);
  PlacesUtils.asContainer(testRootNode);
  testRootNode.containerOpen = true;
  is(testRootNode.childCount, 0, "confirm test root node is a container, and is empty");

  // create folder A, fill it w/ each item type
  var folderAId = PlacesUtils.bookmarks.createFolder(testRootId, "A", -1);
  PlacesUtils.bookmarks.insertBookmark(folderAId, PlacesUtils._uri("http://foo"),
                                       -1, "test bookmark");
  PlacesUtils.bookmarks.insertSeparator(folderAId, -1);
  var folderANode = testRootNode.getChild(0);
  var folderAGUIDs = getGUIDs(folderANode);

  // test the test function
  ok(checkGUIDs(folderANode, folderAGUIDs, true), "confirm guid test works");

  // serialize the folder
  var serializedNode = PlacesUtils.wrapNode(folderANode, PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER);
  var rawNode = PlacesUtils.unwrapNodes(serializedNode, PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER).shift();
  ok(rawNode.type, "confirm json node was made");

  // Create a copy transaction from the serialization.
  // this exercises the guid-filtering
  var transaction = PlacesUIUtils.makeTransaction(rawNode,
                                                  PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER,
                                                  testRootId, -1, true);
  ok(transaction, "create transaction");

  // execute it, copying to the test root folder
  PlacesUtils.transactionManager.doTransaction(transaction);
  is(testRootNode.childCount, 2, "create test folder via copy");

  // check GUIDs are different
  var folderBNode = testRootNode.getChild(1);
  ok(checkGUIDs(folderBNode, folderAGUIDs, false), "confirm folder A GUIDs don't match folder B GUIDs");
  var folderBGUIDs = getGUIDs(folderBNode);
  ok(checkGUIDs(folderBNode, folderBGUIDs, true), "confirm test of new GUIDs");

  // undo the transaction, confirm the removal
  PlacesUtils.transactionManager.undoTransaction();
  is(testRootNode.childCount, 1, "confirm undo removed the copied folder");

  // redo the transaction
  // confirming GUIDs persist through undo/redo
  PlacesUtils.transactionManager.redoTransaction();
  is(testRootNode.childCount, 2, "confirm redo re-copied the folder");
  folderBNode = testRootNode.getChild(1);
  ok(checkGUIDs(folderBNode, folderAGUIDs, false), "folder B GUIDs after undo/redo don't match folder A GUIDs"); // sanity check
  ok(checkGUIDs(folderBNode, folderBGUIDs, true), "folder B GUIDs after under/redo should match pre-undo/redo folder B GUIDs");

  // Close containers, cleaning up their observers.
  testRootNode.containerOpen = false;
  toolbarNode.containerOpen = false;

  // clean up
  PlacesUtils.transactionManager.undoTransaction();
  PlacesUtils.bookmarks.removeItem(testRootId);
}

function getGUIDs(aNode) {
  PlacesUtils.asContainer(aNode);
  aNode.containerOpen = true;
  var GUIDs = {
    folder: PlacesUtils.bookmarks.getItemGUID(aNode.itemId),
    bookmark: PlacesUtils.bookmarks.getItemGUID(aNode.getChild(0).itemId),
    separator: PlacesUtils.bookmarks.getItemGUID(aNode.getChild(1).itemId)
  };
  aNode.containerOpen = false;
  return GUIDs;
}

function checkGUIDs(aFolderNode, aGUIDs, aShouldMatch) {

  function check(aNode, aGUID, aEquals) {
    var nodeGUID = PlacesUtils.bookmarks.getItemGUID(aNode.itemId);
    return aEquals ? (nodeGUID == aGUID) : (nodeGUID != aGUID);
  }

  PlacesUtils.asContainer(aFolderNode);
  aFolderNode.containerOpen = true;

  var allMatch = check(aFolderNode, aGUIDs.folder, aShouldMatch) &&
                 check(aFolderNode.getChild(0), aGUIDs.bookmark, aShouldMatch) &&
                 check(aFolderNode.getChild(1), aGUIDs.separator, aShouldMatch)

  aFolderNode.containerOpen = false;
  return allMatch;
}
