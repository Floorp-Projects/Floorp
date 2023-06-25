"use strict";

const TEST_URL = "https://www.example.com/";
const testTag = "foo";
const testTagUpper = "Foo";
const testURI = Services.io.newURI(TEST_URL);

add_task(async function test() {
  // Add a bookmark.
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    title: "mozilla",
    url: testURI,
  });

  PlacesUtils.tagging.tagURI(makeURI(TEST_URL), ["tag0"]);

  const win = await BrowserTestUtils.openNewBrowserWindow();
  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
  });
  win.StarUI._createPanelIfNeeded();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser: win.gBrowser,
      url: TEST_URL,
    },
    async () => {
      // Init panel
      await TestUtils.waitForCondition(
        () => BookmarkingUI.status !== BookmarkingUI.STATUS_UPDATING
      );

      await clickBookmarkStar(win);

      // add a tag
      await fillBookmarkTextField("editBMPanel_tagsField", testTag, win);

      let promiseNotification = PlacesTestUtils.waitForNotification(
        "bookmark-tags-changed"
      );
      await hideBookmarksPanel(win);
      await promiseNotification;

      // test that the tag has been added in the backend
      is(PlacesUtils.tagging.getTagsForURI(testURI)[0], testTag, "tags match");

      // change the tag
      await TestUtils.waitForCondition(
        () => BookmarkingUI.status !== BookmarkingUI.STATUS_UPDATING
      );
      await clickBookmarkStar(win);
      await fillBookmarkTextField("editBMPanel_tagsField", testTagUpper, win);
      // The old sync API doesn't notify a tags change, and fixing it would be
      // quite complex, so we just wait for a title change until tags are
      // refactored.
      promiseNotification = PlacesTestUtils.waitForNotification(
        "bookmark-title-changed"
      );
      await hideBookmarksPanel(win);
      await promiseNotification;

      // test that the tag has been added in the backend
      is(
        PlacesUtils.tagging.getTagsForURI(testURI)[0],
        testTagUpper,
        "tags match"
      );

      // Cleanup.
      PlacesUtils.tagging.untagURI(testURI, [testTag]);
      await PlacesUtils.bookmarks.remove(bm.guid);
    }
  );
});
