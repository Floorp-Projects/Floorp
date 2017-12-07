/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testURL = "data:text/plain,nothing but plain text";
var testTag = "581253_tag";

add_task(async function test_remove_bookmark_with_tag_via_edit_bookmark() {
  waitForExplicitFinish();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.eraseEverything();
    await BrowserTestUtils.removeTab(tab);
    await PlacesTestUtils.clearHistory();
  });

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "",
    url: testURL,
  });

  Assert.ok(await PlacesUtils.bookmarks.fetch({url: testURL}), "the test url is bookmarked");

  // eslint-disable-next-line mozilla/no-cpows-in-tests
  content.location = testURL;

  await BrowserTestUtils.waitForCondition(
    () => BookmarkingUI.status == BookmarkingUI.STATUS_STARRED,
    "star button indicates that the page is bookmarked");

  PlacesUtils.tagging.tagURI(makeURI(testURL), [testTag]);

  let popupShownPromise = BrowserTestUtils.waitForEvent(StarUI.panel, "popupshown");

  BookmarkingUI.star.click();

  await popupShownPromise;

  let tagsField = document.getElementById("editBMPanel_tagsField");
  Assert.ok(tagsField.value == testTag, "tags field value was set");
  tagsField.focus();

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(StarUI.panel, "popuphidden");

  let removeNotification = PlacesTestUtils.waitForNotification("onItemRemoved",
     (id, parentId, index, type, itemUrl) => testURL == unescape(itemUrl.spec));

  let removeButton = document.getElementById("editBookmarkPanelRemoveButton");
  removeButton.click();

  await popupHiddenPromise;

  await removeNotification;

  is(BookmarkingUI.status, BookmarkingUI.STATUS_UNSTARRED,
     "star button indicates that the bookmark has been removed");
});
