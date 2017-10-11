/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "http://example.com/";
const TEST_URL1 = "https://example.com/otherbrowser";

var PlacesOrganizer;
var ContentTree;
var bookmark;
var bookmarkId;

add_task(async function setup() {
  await PlacesUtils.bookmarks.eraseEverything();
  let organizer = await promiseLibrary();

  registerCleanupFunction(async function() {
    await promiseLibraryClosed(organizer);
    await PlacesUtils.bookmarks.eraseEverything();
  });

  PlacesOrganizer = organizer.PlacesOrganizer;
  ContentTree = organizer.ContentTree;

  info("Selecting BookmarksToolbar in the left pane");
  PlacesOrganizer.selectLeftPaneQuery("BookmarksToolbar");

  bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: TEST_URL,
    title: "0"
  });
  bookmarkId = await PlacesUtils.promiseItemId(bookmark.guid);

  ContentTree.view.selectItems([bookmarkId]);

  await promiseClipboard(() => {
    info("Copying selection");
    ContentTree.view.controller.cut();
  }, PlacesUtils.TYPE_X_MOZ_PLACE);
});

add_task(async function paste() {
  info("Selecting UnfiledBookmarks in the left pane");
  PlacesOrganizer.selectLeftPaneQuery("UnfiledBookmarks");

  info("Pasting clipboard");
  await ContentTree.view.controller.paste();

  let tree = await PlacesUtils.promiseBookmarksTree(PlacesUtils.bookmarks.unfiledGuid);

  Assert.equal(tree.children.length, 1,
               "Should be one bookmark in the unfiled folder.");
  Assert.equal(tree.children[0].title, "0",
               "Should have the correct title");
  Assert.equal(tree.children[0].uri, TEST_URL,
               "Should have the correct URL");

  await PlacesUtils.bookmarks.remove(tree.children[0].guid);
});

add_task(async function paste_from_different_instance() {
  let xferable = Cc["@mozilla.org/widget/transferable;1"]
                   .createInstance(Ci.nsITransferable);
  xferable.init(null);

  // Fake data on the clipboard to pretend this is from a different instance
  // of Firefox.
  let data = {
    "title": "test",
    "id": 32,
    "instanceId": "FAKEFAKEFAKE",
    "itemGuid": "ZBf_TYkrYGvW",
    "parent": 452,
    "dateAdded": 1464866275853000,
    "lastModified": 1507638113352000,
    "type": "text/x-moz-place",
    "uri": TEST_URL1
  };
  data = JSON.stringify(data);

  xferable.addDataFlavor(PlacesUtils.TYPE_X_MOZ_PLACE);
  xferable.setTransferData(PlacesUtils.TYPE_X_MOZ_PLACE, PlacesUtils.toISupportsString(data),
                           data.length * 2);

  Services.clipboard.setData(xferable, null, Ci.nsIClipboard.kGlobalClipboard);

  info("Pasting clipboard");

  await ContentTree.view.controller.paste();

  let tree = await PlacesUtils.promiseBookmarksTree(PlacesUtils.bookmarks.unfiledGuid);

  Assert.equal(tree.children.length, 1,
               "Should be one bookmark in the unfiled folder.");
  Assert.equal(tree.children[0].title, "test",
               "Should have the correct title");
  Assert.equal(tree.children[0].uri, TEST_URL1,
               "Should have the correct URL");

  await PlacesUtils.bookmarks.remove(tree.children[0].guid);
});
