/**
 *  Test that tags can be added to bookmarks using the star-shaped button.
 */
"use strict";

let bookmarkPanel = document.getElementById("editBookmarkPanel");
let doneButton = document.getElementById("editBookmarkPanelDoneButton");
let bookmarkStar = BookmarkingUI.star;
let bookmarkPanelTitle = document.getElementById("editBookmarkPanelTitle");
let TEST_URL = "about:buildconfig";

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

add_task(async function test_add_tags() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_URL,
    waitForStateStop: true
  });

  // The bookmarks panel is expected to auto-close after this step.
  await hideBookmarksPanel(async () => {
    // Click the bookmark star to bookmark the page.
    await clickBookmarkStar();
    Assert.equal(bookmarkPanelTitle.value, gNavigatorBundle.getString("editBookmarkPanel.pageBookmarkedTitle"), "Bookmark title is correct");
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
  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.eraseEverything();
    await BrowserTestUtils.removeTab(tab);
  });
});
