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
  ok(PlacesControllerDragHelper, "checking PlacesControllerDragHelper, running in chrome context?");

  const IDX = PlacesUtils.bookmarks.DEFAULT_INDEX;

  // setup
  var rootId = PlacesUtils.bookmarks.createFolder(PlacesUtils.toolbarFolderId, "", IDX);
  var rootNode = PlacesUtils.getFolderContents(rootId, false, true).root;
  is(rootNode.childCount, 0, "confirm test root is empty");

  var tests = [];

  // add a regular folder, should be moveable
  tests.push({
    populate: function() {
      this.id =
        PlacesUtils.bookmarks.createFolder(rootId, "", IDX);
    },
    validate: function() {
      is(rootNode.childCount, 1,
        "populate added data to the test root");
      is(PlacesControllerDragHelper.canMoveContainer(this.id),
         true, "can move regular folder id");
      is(PlacesControllerDragHelper.canMoveNode(rootNode.getChild(0)),
         true, "can move regular folder node");
    }
  });

  // add a regular folder shortcut, should be moveable
  tests.push({
    populate: function() {
      this.folderId =
        PlacesUtils.bookmarks.createFolder(rootId, "foo", IDX);
      this.shortcutId =
        PlacesUtils.bookmarks.insertBookmark(rootId, makeURI("place:folder="+this.folderId), IDX, "bar");
    },
    validate: function() {
      is(rootNode.childCount, 2,
        "populated data to the test root");

      var folderNode = rootNode.getChild(0);
      is(folderNode.type, 6, "node is folder");
      is(this.folderId, folderNode.itemId, "folder id and folder node item id match");

      var shortcutNode = rootNode.getChild(1);
      is(shortcutNode.type, 9, "node is folder shortcut");
      is(this.shortcutId, shortcutNode.itemId, "shortcut id and shortcut node item id match");

      var concreteId = PlacesUtils.getConcreteItemId(shortcutNode);
      is(concreteId, folderNode.itemId, "shortcut node id and concrete id match");

      is(PlacesControllerDragHelper.canMoveContainer(this.shortcutId),
         true, "can move folder shortcut id");

      is(PlacesControllerDragHelper.canMoveNode(shortcutNode),
         true, "can move folder shortcut node");
    }
  });

  // add a regular query, should be moveable
  tests.push({
    populate: function() {
      this.bookmarkId =
        PlacesUtils.bookmarks.insertBookmark(rootId, makeURI("http://foo.com"), IDX, "foo");
      this.queryId =
        PlacesUtils.bookmarks.insertBookmark(rootId, makeURI("place:terms=foo"), IDX, "bar");
    },
    validate: function() {
      is(rootNode.childCount, 2,
        "populated data to the test root");

      var bmNode = rootNode.getChild(0);
      is(bmNode.itemId, this.bookmarkId, "bookmark id and bookmark node item id match");

      var queryNode = rootNode.getChild(1);
      is(queryNode.itemId, this.queryId, "query id and query node item id match");

      is(PlacesControllerDragHelper.canMoveContainer(this.queryId),
         true, "can move query id");

      is(PlacesControllerDragHelper.canMoveNode(queryNode),
         true, "can move query node");
    }
  });

  // test that special folders cannot be moved
  // test that special folders shortcuts can be moved
  tests.push({
    folders: [PlacesUtils.bookmarksMenuFolderId,
              PlacesUtils.tagsFolderId, PlacesUtils.unfiledBookmarksFolderId,
              PlacesUtils.toolbarFolderId],
    shortcuts: {},
    populate: function() {
      for (var i = 0; i < this.folders.length; i++) {
        var id = this.folders[i];
        this.shortcuts[id] =
          PlacesUtils.bookmarks.insertBookmark(rootId, makeURI("place:folder=" + id), IDX, "");
      }
    },
    validate: function() {
      // test toolbar shortcut node
      is(rootNode.childCount, this.folders.length,
        "populated data to the test root");

      function getRootChildNode(aId) {
        var node = PlacesUtils.getFolderContents(PlacesUtils.placesRootId, false, true).root;
        for (var i = 0; i < node.childCount; i++) {
          var child = node.getChild(i);
          if (child.itemId == aId) {
            node.containerOpen = false;
            return child;
          }
        }
        node.containerOpen = false;
        throw("Unable to find child node");
        return null;
      }

      for (var i = 0; i < this.folders.length; i++) {
        var id = this.folders[i];

        is(PlacesControllerDragHelper.canMoveContainer(id),
           false, "shouldn't be able to move special folder id");

        var node = getRootChildNode(id);
        isnot(node, null, "Node found");
        is(PlacesControllerDragHelper.canMoveNode(node),
           false, "shouldn't be able to move special folder node");

        var shortcutId = this.shortcuts[id];
        var shortcutNode = rootNode.getChild(i);

        is(shortcutNode.itemId, shortcutId, "shortcut id and shortcut node item id match");

        LOG("can move shortcut id?");
        is(PlacesControllerDragHelper.canMoveContainer(shortcutId),
           true, "should be able to move special folder shortcut id");

        LOG("can move shortcut node?");
        is(PlacesControllerDragHelper.canMoveNode(shortcutNode),
           true, "should be able to move special folder shortcut node");
      }
    }
  });

  // test that a tag container cannot be moved
  tests.push({
    populate: function() {
      // tag a uri
      this.uri = makeURI("http://foo.com");
      PlacesUtils.tagging.tagURI(this.uri, ["bar"]);
    },
    validate: function() {
      // get tag root
      var query = PlacesUtils.history.getNewQuery();
      var options = PlacesUtils.history.getNewQueryOptions();
      options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_QUERY;
      var tagsNode = PlacesUtils.history.executeQuery(query, options).root;

      tagsNode.containerOpen = true;
      is(tagsNode.childCount, 1, "has new tag");

      var tagNode = tagsNode.getChild(0);
      
      is(PlacesControllerDragHelper.canMoveNode(tagNode),
         false, "should not be able to move tag container node");

      tagsNode.containerOpen = false;
    }
  });

  // test that any child of a read-only node cannot be moved
  tests.push({
    populate: function() {
      this.id =
        PlacesUtils.bookmarks.createFolder(rootId, "foo", IDX);
      PlacesUtils.bookmarks.createFolder(this.id, "bar", IDX);
      PlacesUtils.bookmarks.setFolderReadonly(this.id, true);
    },
    validate: function() {
      is(rootNode.childCount, 1,
        "populate added data to the test root");
      var readOnlyFolder = rootNode.getChild(0);

      // test that we can move the read-only folder
      is(PlacesControllerDragHelper.canMoveContainer(this.id),
         true, "can move read-only folder id");
      is(PlacesControllerDragHelper.canMoveNode(readOnlyFolder),
         true, "can move read-only folder node");

      // test that we cannot move the child of a read-only folder
      readOnlyFolder.QueryInterface(Ci.nsINavHistoryContainerResultNode);
      readOnlyFolder.containerOpen = true;
      var childFolder = readOnlyFolder.getChild(0);

      is(PlacesControllerDragHelper.canMoveContainer(childFolder.itemId),
         false, "cannot move a child of a read-only folder");
      is(PlacesControllerDragHelper.canMoveNode(childFolder),
         false, "cannot move a child node of a read-only folder node");
    }
  });

  tests.forEach(function(aTest) {
    PlacesUtils.bookmarks.removeFolderChildren(rootId);
    aTest.populate();
    aTest.validate();
  });

  PlacesUtils.bookmarks.removeItem(rootId);
}
