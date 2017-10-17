/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function() {
  let root = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "",
    type: PlacesUtils.bookmarks.TYPE_FOLDER
  });

  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
  });

  await withSidebarTree("bookmarks", async function(tree) {
    info("Test a regular folder");
    let folder = await PlacesUtils.bookmarks.insert({
      parentGuid: root.guid,
      title: "",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
    });
    let folderId = await PlacesUtils.promiseItemId(folder.guid);
    tree.selectItems([folderId]);
    Assert.equal(tree.selectedNode.bookmarkGuid, folder.guid,
                 "Selected the expected node");
    Assert.equal(tree.selectedNode.type, 6, "node is a folder");
    Assert.ok(PlacesControllerDragHelper.canMoveNode(tree.selectedNode, tree),
              "can move regular folder node");

    info("Test a folder shortcut");
    let shortcut = await PlacesUtils.bookmarks.insert({
      parentGuid: root.guid,
      title: "bar",
      url: `place:folder=${folderId}`
    });
    let shortcutId = await PlacesUtils.promiseItemId(shortcut.guid);
    tree.selectItems([shortcutId]);
    Assert.equal(tree.selectedNode.bookmarkGuid, shortcut.guid,
                 "Selected the expected node");
    Assert.equal(tree.selectedNode.type, 9, "node is a folder shortcut");
    Assert.equal(PlacesUtils.getConcreteItemGuid(tree.selectedNode),
                 folder.guid, "shortcut node guid and concrete guid match");
    Assert.ok(PlacesControllerDragHelper.canMoveNode(tree.selectedNode, tree),
              "can move folder shortcut node");

    info("Test a query");
    let bookmark = await PlacesUtils.bookmarks.insert({
      parentGuid: root.guid,
      title: "",
      url: "http://foo.com",
    });
    let bookmarkId = await PlacesUtils.promiseItemId(bookmark.guid);
    tree.selectItems([bookmarkId]);
    Assert.equal(tree.selectedNode.bookmarkGuid, bookmark.guid,
                 "Selected the expected node");
    let query = await PlacesUtils.bookmarks.insert({
      parentGuid: root.guid,
      title: "bar",
      url: `place:terms=foo`
    });
    let queryId = await PlacesUtils.promiseItemId(query.guid);
    tree.selectItems([queryId]);
    Assert.equal(tree.selectedNode.bookmarkGuid, query.guid,
                 "Selected the expected node");
    Assert.ok(PlacesControllerDragHelper.canMoveNode(tree.selectedNode, tree),
              "can move query node");


    info("Test a tag container");
    PlacesUtils.tagging.tagURI(Services.io.newURI(bookmark.url.href), ["bar"]);
    // Add the tags root query.
    let tagsQuery = await PlacesUtils.bookmarks.insert({
      parentGuid: root.guid,
      title: "",
      url: "place:type=" + Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_QUERY,
    });
    let tagsQueryId = await PlacesUtils.promiseItemId(tagsQuery.guid);
    tree.selectItems([tagsQueryId]);
    PlacesUtils.asQuery(tree.selectedNode).containerOpen = true;
    Assert.equal(tree.selectedNode.childCount, 1, "has tags");
    let tagNode = tree.selectedNode.getChild(0);
    Assert.ok(!PlacesControllerDragHelper.canMoveNode(tagNode, tree),
              "should not be able to move tag container node");
    tree.selectedNode.containerOpen = false;

    info("Test that special folders and cannot be moved but other shortcuts can.");
    let roots = [
      PlacesUtils.bookmarksMenuFolderId,
      PlacesUtils.unfiledBookmarksFolderId,
      PlacesUtils.toolbarFolderId,
    ];

    for (let id of roots) {
      selectShortcutForRootId(tree, id);
      Assert.ok(!PlacesControllerDragHelper.canMoveNode(tree.selectedNode, tree),
                "shouldn't be able to move default shortcuts to roots");
      let s = await PlacesUtils.bookmarks.insert({
        parentGuid: root.guid,
        title: "bar",
        url: `place:folder=${id}`,
      });
      let sid = await PlacesUtils.promiseItemId(s.guid);
      tree.selectItems([sid]);
      Assert.equal(tree.selectedNode.bookmarkGuid, s.guid,
                   "Selected the expected node");
      Assert.ok(PlacesControllerDragHelper.canMoveNode(tree.selectedNode, tree),
                "should be able to move user-created shortcuts to roots");
    }
  });
});

function selectShortcutForRootId(tree, id) {
  for (let i = 0; i < tree.result.root.childCount; ++i) {
    let child = tree.result.root.getChild(i);
    if (PlacesUtils.getConcreteItemId(child) == id) {
      tree.selectItems([child.itemId]);
      return;
    }
  }
  Assert.ok(false, "Cannot find shortcut to root");
}
