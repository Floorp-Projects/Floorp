/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let rootFolder;
let rootNode;

add_task(async function setup() {
  rootFolder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });

  let rootId = await PlacesUtils.promiseItemId(rootFolder.guid);
  rootNode = PlacesUtils.getFolderContents(rootId, false, true).root;

  Assert.equal(rootNode.childCount, 0, "confirm test root is empty");

  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function test_regular_folder() {
  let regularFolder = await PlacesUtils.bookmarks.insert({
    parentGuid: rootFolder.guid,
    title: "",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });

  Assert.equal(rootNode.childCount, 1,
    "populate added data to the test root");
  Assert.equal(PlacesControllerDragHelper.canMoveNode(rootNode.getChild(0)),
     true, "can move regular folder node");

  await PlacesUtils.bookmarks.remove(regularFolder);
});

add_task(async function test_folder_shortcut() {
  let regularFolder = await PlacesUtils.bookmarks.insert({
    parentGuid: rootFolder.guid,
    title: "",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });
  let regularFolderId = await PlacesUtils.promiseItemId(regularFolder.guid);

  let shortcut = await PlacesUtils.bookmarks.insert({
    parentGuid: rootFolder.guid,
    title: "bar",
    url: `place:folder=${regularFolderId}`
  });

  Assert.equal(rootNode.childCount, 2,
    "populated data to the test root");

  let folderNode = rootNode.getChild(0);
  Assert.equal(folderNode.type, 6, "node is folder");
  Assert.equal(regularFolder.guid, folderNode.bookmarkGuid, "folder guid and folder node item guid match");

  let shortcutNode = rootNode.getChild(1);
  Assert.equal(shortcutNode.type, 9, "node is folder shortcut");
  Assert.equal(shortcut.guid, shortcutNode.bookmarkGuid, "shortcut guid and shortcut node item guid match");

  let concreteId = PlacesUtils.getConcreteItemGuid(shortcutNode);
  Assert.equal(concreteId, folderNode.bookmarkGuid, "shortcut node id and concrete id match");

  Assert.equal(PlacesControllerDragHelper.canMoveNode(shortcutNode), true,
   "can move folder shortcut node");

  await PlacesUtils.bookmarks.remove(shortcut);
  await PlacesUtils.bookmarks.remove(regularFolder);
});

add_task(async function test_regular_query() {
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: rootFolder.guid,
    title: "",
    url: "http://foo.com",
  });

  let query = await PlacesUtils.bookmarks.insert({
    parentGuid: rootFolder.guid,
    title: "bar",
    url: `place:terms=foo`
  });

  Assert.equal(rootNode.childCount, 2,
    "populated data to the test root");

  let bmNode = rootNode.getChild(0);
  Assert.equal(bmNode.bookmarkGuid, bookmark.guid, "bookmark guid and bookmark node item guid match");

  let queryNode = rootNode.getChild(1);
  Assert.equal(queryNode.bookmarkGuid, query.guid, "query guid and query node item guid match");

  Assert.equal(PlacesControllerDragHelper.canMoveNode(queryNode),
     true, "can move query node");

  await PlacesUtils.bookmarks.remove(query);
  await PlacesUtils.bookmarks.remove(bookmark);
});

add_task(async function test_special_folders() {
  // Test that special folders and special folder shortcuts cannot be moved.
  let folders = [
    PlacesUtils.bookmarksMenuFolderId,
    PlacesUtils.tagsFolderId,
    PlacesUtils.unfiledBookmarksFolderId,
    PlacesUtils.toolbarFolderId
  ];

  let children = folders.map(folderId => {
    return {
      title: "",
      url: `place:folder=${folderId}`
    };
  });

  let shortcuts = await PlacesUtils.bookmarks.insertTree({
    guid: rootFolder.guid,
    children
  });

  // test toolbar shortcut node
  Assert.equal(rootNode.childCount, folders.length,
    "populated data to the test root");

  function getRootChildNode(aId) {
    let node = PlacesUtils.getFolderContents(PlacesUtils.placesRootId, false, true).root;
    for (let i = 0; i < node.childCount; i++) {
      let child = node.getChild(i);
      if (child.itemId == aId) {
        node.containerOpen = false;
        return child;
      }
    }
    node.containerOpen = false;
    ok(false, "Unable to find child node");
    return null;
  }

  for (let i = 0; i < folders.length; i++) {
    let id = folders[i];

    let node = getRootChildNode(id);
    isnot(node, null, "Node found");
    Assert.equal(PlacesControllerDragHelper.canMoveNode(node),
       false, "shouldn't be able to move special folder node");

    let shortcut = shortcuts[i];
    let shortcutNode = rootNode.getChild(i);

    Assert.equal(shortcutNode.bookmarkGuid, shortcut.guid,
      "shortcut guid and shortcut node item guid match");

    Assert.equal(PlacesControllerDragHelper.canMoveNode(shortcutNode),
       true, "should be able to move special folder shortcut node");
  }
});

add_task(async function test_tag_container() {
  // tag a uri
  this.uri = makeURI("http://foo.com");
  PlacesUtils.tagging.tagURI(this.uri, ["bar"]);
  registerCleanupFunction(() => PlacesUtils.tagging.untagURI(this.uri, ["bar"]));

  // get tag root
  let query = PlacesUtils.history.getNewQuery();
  let options = PlacesUtils.history.getNewQueryOptions();
  options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_QUERY;
  let tagsNode = PlacesUtils.history.executeQuery(query, options).root;

  tagsNode.containerOpen = true;
  Assert.equal(tagsNode.childCount, 1, "has new tag");

  let tagNode = tagsNode.getChild(0);

  Assert.equal(PlacesControllerDragHelper.canMoveNode(tagNode),
     false, "should not be able to move tag container node");
  tagsNode.containerOpen = false;
});
