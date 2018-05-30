/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 "use strict";

/**
 * Tests that multiple tags can be added to a bookmark using the star-shaped button, the library and the sidebar.
 */

const bookmarkPanel = document.getElementById("editBookmarkPanel");
const bookmarkStar = BookmarkingUI.star;

async function clickBookmarkStar() {
  let shownPromise = promisePopupShown(bookmarkPanel);
  bookmarkStar.click();
  await shownPromise;
}

async function hideBookmarksPanel(callback) {
  let hiddenPromise = promisePopupHidden(bookmarkPanel);
  callback();
  await hiddenPromise;
}

add_task(async function test_add_bookmark_tags_from_bookmarkProperties() {
  const TEST_URL = "about:robots";

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_URL,
    waitForStateStop: true
  });

  // Cleanup.
  registerCleanupFunction(async function() {
    BrowserTestUtils.removeTab(tab);
  });

  let bookmarkPanelTitle = document.getElementById("editBookmarkPanelTitle");

  // The bookmarks panel is expected to auto-close after this step.
  await hideBookmarksPanel(async () => {
    // Click the bookmark star to bookmark the page.
    await clickBookmarkStar();
    Assert.equal(bookmarkPanelTitle.value, gNavigatorBundle.getString("editBookmarkPanel.newBookmarkTitle"), "Bookmark title is correct");
    Assert.equal(bookmarkStar.getAttribute("starred"), "true", "Page is starred");
  });

  // Click the bookmark star again to add tags.
  await clickBookmarkStar();
  Assert.equal(bookmarkPanelTitle.value, gNavigatorBundle.getString("editBookmarkPanel.editBookmarkTitle"), "Bookmark title is correct");
  let promiseNotification = PlacesTestUtils.waitForNotification("onItemAdded", (id, parentId, index, type, itemUrl) => {
    if (itemUrl !== null) {
      return itemUrl.equals(Services.io.newURI(TEST_URL));
    }
    return true;
  });
  await fillBookmarkTextField("editBMPanel_tagsField", "tag1", window);
  await promiseNotification;
  let bookmarks = [];
  await PlacesUtils.bookmarks.fetch({ url: TEST_URL }, bm => bookmarks.push(bm));
  Assert.equal(PlacesUtils.tagging.getTagsForURI(Services.io.newURI(TEST_URL)).length, 1, "Found the right number of tags");
  Assert.deepEqual(PlacesUtils.tagging.getTagsForURI(Services.io.newURI(TEST_URL)), ["tag1"]);
  let doneButton = document.getElementById("editBookmarkPanelDoneButton");
  await hideBookmarksPanel(() => doneButton.click());

  // Click the bookmark star again, add more tags.
  await clickBookmarkStar();
  promiseNotification = PlacesTestUtils.waitForNotification("onItemChanged", (id, property) => property == "tags");
  await fillBookmarkTextField("editBMPanel_tagsField", "tag1, tag2, tag3", window);
  await promiseNotification;
  await hideBookmarksPanel(() => doneButton.click());

  bookmarks = [];
  await PlacesUtils.bookmarks.fetch({ url: TEST_URL }, bm => bookmarks.push(bm));
  Assert.equal(bookmarks.length, 1, "Only one bookmark should exist");
  Assert.equal(PlacesUtils.tagging.getTagsForURI(Services.io.newURI(TEST_URL)).length, 3, "Found the right number of tags");
  Assert.deepEqual(PlacesUtils.tagging.getTagsForURI(Services.io.newURI(TEST_URL)), ["tag1", "tag2", "tag3"]);

  // Cleanup.
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_add_bookmark_tags_from_library() {
  const uri = "http://example.com/";

  // Add a bookmark.
  await PlacesUtils.bookmarks.insert({
    url: uri,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid
  });

  // Open the Library on "UnfiledBookmarks".
  let library = await promiseLibrary("UnfiledBookmarks");

  // Cleanup.
  registerCleanupFunction(async function() {
    await promiseLibraryClosed(library);
  });

  let bookmarkNode = library.ContentTree.view.selectedNode;
  Assert.equal(bookmarkNode.uri, "http://example.com/", "Found the expected bookmark");

  // Add a tag to the bookmark.
  fillBookmarkTextField("editBMPanel_tagsField", "tag1", library);

  await waitForCondition(() => bookmarkNode.tags === "tag1", "Node tag is correct");

  // Add a new tag to the bookmark.
  fillBookmarkTextField("editBMPanel_tagsField", "tag1, tag2", library);

  await waitForCondition(() => bookmarkNode.tags === "tag1, tag2", "Node tag is correct");

  // Check the tag change has been completed.
  let tags = PlacesUtils.tagging.getTagsForURI(Services.io.newURI(uri));
  Assert.equal(tags.length, 2, "Found the right number of tags");
  Assert.deepEqual(tags, ["tag1", "tag2"], "Found the expected tags");

  // Cleanup.
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_add_bookmark_tags_from_sidebar() {
  const TEST_URL = "about:buildconfig";

  let bookmarks = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URL,
    title: "Bookmark Title"
  });

  await withSidebarTree("bookmarks", async function(tree) {
    tree.selectItems([bookmarks.guid]);
    // Add one tag.
    await addTags(["tag1"], tree, ["tag1"]);
    // Add 2 more tags.
    await addTags(["tag2", "tag3"], tree, ["tag1", "tag2", "tag3"]);
  });

  async function addTags(tagValue, tree, expected) {
    await withBookmarksDialog(
      false,
      function openPropertiesDialog() {
        tree.controller.doCommand("placesCmd_show:info");
      },
      async function test(dialogWin) {
        PlacesUtils.tagging.tagURI(makeURI(TEST_URL), tagValue);
        let tags = PlacesUtils.tagging.getTagsForURI(Services.io.newURI(TEST_URL));

        Assert.deepEqual(tags, expected, "Tags field is correctly populated");

        EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
      });
  }

  // Cleanup.
  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
  });
});
