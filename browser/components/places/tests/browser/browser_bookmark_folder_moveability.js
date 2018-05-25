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
    tree.selectItems([folder.guid]);
    Assert.equal(tree.selectedNode.bookmarkGuid, folder.guid,
                 "Selected the expected node");
    Assert.equal(tree.selectedNode.type, 6, "node is a folder");
    Assert.ok(tree.controller.canMoveNode(tree.selectedNode),
              "can move regular folder node");

    info("Test a folder shortcut");
    let shortcut = await PlacesUtils.bookmarks.insert({
      parentGuid: root.guid,
      title: "bar",
      url: `place:parent=${folder.guid}`
    });
    tree.selectItems([shortcut.guid]);
    Assert.equal(tree.selectedNode.bookmarkGuid, shortcut.guid,
                 "Selected the expected node");
    Assert.equal(tree.selectedNode.type, 9, "node is a folder shortcut");
    Assert.equal(PlacesUtils.getConcreteItemGuid(tree.selectedNode),
                 folder.guid, "shortcut node guid and concrete guid match");
    Assert.ok(tree.controller.canMoveNode(tree.selectedNode),
              "can move folder shortcut node");

    info("Test a query");
    let bookmark = await PlacesUtils.bookmarks.insert({
      parentGuid: root.guid,
      title: "",
      url: "http://foo.com",
    });
    tree.selectItems([bookmark.guid]);
    Assert.equal(tree.selectedNode.bookmarkGuid, bookmark.guid,
                 "Selected the expected node");
    let query = await PlacesUtils.bookmarks.insert({
      parentGuid: root.guid,
      title: "bar",
      url: `place:terms=foo`
    });
    tree.selectItems([query.guid]);
    Assert.equal(tree.selectedNode.bookmarkGuid, query.guid,
                 "Selected the expected node");
    Assert.ok(tree.controller.canMoveNode(tree.selectedNode),
              "can move query node");


    info("Test a tag container");
    PlacesUtils.tagging.tagURI(Services.io.newURI(bookmark.url.href), ["bar"]);
    // Add the tags root query.
    let tagsQuery = await PlacesUtils.bookmarks.insert({
      parentGuid: root.guid,
      title: "",
      url: "place:type=" + Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAGS_ROOT,
    });
    tree.selectItems([tagsQuery.guid]);
    PlacesUtils.asQuery(tree.selectedNode).containerOpen = true;
    Assert.equal(tree.selectedNode.childCount, 1, "has tags");
    let tagNode = tree.selectedNode.getChild(0);
    Assert.ok(!tree.controller.canMoveNode(tagNode),
              "should not be able to move tag container node");
    tree.selectedNode.containerOpen = false;

    info("Test that special folders and cannot be moved but other shortcuts can.");
    let roots = [
      PlacesUtils.bookmarks.menuGuid,
      PlacesUtils.bookmarks.unfiledGuid,
      PlacesUtils.bookmarks.toolbarGuid,
    ];

    for (let guid of roots) {
      tree.selectItems([guid]);
      Assert.ok(!tree.controller.canMoveNode(tree.selectedNode),
                "shouldn't be able to move default shortcuts to roots");
      let s = await PlacesUtils.bookmarks.insert({
        parentGuid: root.guid,
        title: "bar",
        url: `place:parent=${guid}`,
      });
      tree.selectItems([s.guid]);
      Assert.equal(tree.selectedNode.bookmarkGuid, s.guid,
                   "Selected the expected node");
      Assert.ok(tree.controller.canMoveNode(tree.selectedNode),
                "should be able to move user-created shortcuts to roots");
    }
  });
});
