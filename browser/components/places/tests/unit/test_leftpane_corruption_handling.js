/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that we build a working leftpane in various corruption situations.
 */

// Used to store the original leftPaneFolderId getter.
var gLeftPaneFolderIdGetter;
var gAllBookmarksFolderIdGetter;
// Used to store the original left Pane status as a JSON string.
var gReferenceHierarchy;
var gLeftPaneFolderId;

add_task(async function() {
  // We want empty roots.
  await PlacesUtils.bookmarks.eraseEverything();

  // Sanity check.
  Assert.ok(!!PlacesUIUtils);

  // Check getters.
  gLeftPaneFolderIdGetter = Object.getOwnPropertyDescriptor(PlacesUIUtils, "leftPaneFolderId");
  Assert.equal(typeof(gLeftPaneFolderIdGetter.get), "function");
  gAllBookmarksFolderIdGetter = Object.getOwnPropertyDescriptor(PlacesUIUtils, "allBookmarksFolderId");
  Assert.equal(typeof(gAllBookmarksFolderIdGetter.get), "function");

  do_register_cleanup(() => PlacesUtils.bookmarks.eraseEverything());
});

add_task(async function() {
  // Add a third party bogus annotated item.  Should not be removed.
  let folder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "test",
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    type: PlacesUtils.bookmarks.TYPE_FOLDER
  });

  let folderId = await PlacesUtils.promiseItemId(folder.guid);
  PlacesUtils.annotations.setItemAnnotation(folderId, ORGANIZER_QUERY_ANNO,
                                            "test", 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);

  // Create the left pane, and store its current status, it will be used
  // as reference value.
  gLeftPaneFolderId = PlacesUIUtils.leftPaneFolderId;
  gReferenceHierarchy = folderIdToHierarchy(gLeftPaneFolderId);

  while (gTests.length) {
    // Run current test.
    await gTests.shift();

    // Regenerate getters.
    Object.defineProperty(PlacesUIUtils, "leftPaneFolderId", gLeftPaneFolderIdGetter);
    gLeftPaneFolderId = PlacesUIUtils.leftPaneFolderId;
    Object.defineProperty(PlacesUIUtils, "allBookmarksFolderId", gAllBookmarksFolderIdGetter);

    // Check the new left pane folder.
    let leftPaneHierarchy = folderIdToHierarchy(gLeftPaneFolderId);
    Assert.equal(gReferenceHierarchy, leftPaneHierarchy);

    folder = await PlacesUtils.bookmarks.fetch({guid: folder.guid});
    Assert.equal(folder.title, "test");
  }
});

// Corruption cases.
var gTests = [

  function test1() {
    print("1. Do nothing, checks test calibration.");
  },

  async function test2() {
    print("2. Delete the left pane folder.");
    let guid = await PlacesUtils.promiseItemGuid(gLeftPaneFolderId);
    await PlacesUtils.bookmarks.remove(guid);
  },

  async function test3() {
    print("3. Delete a child of the left pane folder.");
    let guid = await PlacesUtils.promiseItemGuid(gLeftPaneFolderId);
    let bm = await PlacesUtils.bookmarks.fetch({parentGuid: guid, index: 0});
    await PlacesUtils.bookmarks.remove(bm.guid);
  },

  async function test4() {
    print("4. Delete AllBookmarks.");
    let guid = await PlacesUtils.promiseItemGuid(PlacesUIUtils.allBookmarksFolderId);
    await PlacesUtils.bookmarks.remove(guid);
  },

  async function test5() {
    print("5. Create a duplicated left pane folder.");
    let folder = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      title: "PlacesRoot",
      index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      type: PlacesUtils.bookmarks.TYPE_FOLDER
    });

    let folderId = await PlacesUtils.promiseItemId(folder.guid);
    PlacesUtils.annotations.setItemAnnotation(folderId, ORGANIZER_FOLDER_ANNO,
                                              "PlacesRoot", 0,
                                              PlacesUtils.annotations.EXPIRE_NEVER);
  },

  async function test6() {
    print("6. Create a duplicated left pane query.");
    let folder = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      title: "AllBookmarks",
      index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      type: PlacesUtils.bookmarks.TYPE_FOLDER
    });

    let folderId = await PlacesUtils.promiseItemId(folder.guid);
    PlacesUtils.annotations.setItemAnnotation(folderId, ORGANIZER_QUERY_ANNO,
                                              "AllBookmarks", 0,
                                              PlacesUtils.annotations.EXPIRE_NEVER);
  },

  function test7() {
    print("7. Remove the left pane folder annotation.");
    PlacesUtils.annotations.removeItemAnnotation(gLeftPaneFolderId,
                                                 ORGANIZER_FOLDER_ANNO);
  },

  function test8() {
    print("8. Remove a left pane query annotation.");
    PlacesUtils.annotations.removeItemAnnotation(PlacesUIUtils.allBookmarksFolderId,
                                                 ORGANIZER_QUERY_ANNO);
  },

  async function test9() {
    print("9. Remove a child of AllBookmarks.");
    let guid = await PlacesUtils.promiseItemGuid(PlacesUIUtils.allBookmarksFolderId);
    let bm = await PlacesUtils.bookmarks.fetch({parentGuid: guid, index: 0});
    await PlacesUtils.bookmarks.remove(bm.guid);
  }

];

/**
 * Convert a folder item id to a JSON representation of it and its contents.
 */
function folderIdToHierarchy(aFolderId) {
  let root = PlacesUtils.getFolderContents(aFolderId).root;
  let hier = JSON.stringify(hierarchyToObj(root));
  root.containerOpen = false;
  return hier;
}

function hierarchyToObj(aNode) {
  let o = {};
  o.title = aNode.title;
  o.annos = PlacesUtils.getAnnotationsForItem(aNode.itemId);
  if (PlacesUtils.nodeIsURI(aNode)) {
    o.uri = aNode.uri;
  } else if (PlacesUtils.nodeIsFolder(aNode)) {
    o.children = [];
    PlacesUtils.asContainer(aNode).containerOpen = true;
    for (let i = 0; i < aNode.childCount; ++i) {
      o.children.push(hierarchyToObj(aNode.getChild(i)));
    }
    aNode.containerOpen = false;
  }
  return o;
}
