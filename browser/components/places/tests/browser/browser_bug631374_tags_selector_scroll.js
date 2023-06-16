/**
 * This test checks that editing tags doesn't scroll the tags selector
 * listbox to wrong positions.
 */

const TEST_URL = "about:buildconfig";

function scrolledIntoView(item, parentItem) {
  let itemRect = item.getBoundingClientRect();
  let parentItemRect = parentItem.getBoundingClientRect();
  let pointInView = y => parentItemRect.top < y && y < parentItemRect.bottom;

  // Partially visible items are also considered visible.
  return pointInView(itemRect.top) || pointInView(itemRect.bottom);
}

add_task(async function runTest() {
  await PlacesUtils.bookmarks.eraseEverything();
  let tags = [
    "a",
    "b",
    "c",
    "d",
    "e",
    "f",
    "g",
    "h",
    "i",
    "l",
    "m",
    "n",
    "o",
    "p",
  ];

  // Add a bookmark and tag it.
  let uri1 = Services.io.newURI(TEST_URL);
  let bm1 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "mozilla",
    url: uri1.spec,
  });
  PlacesUtils.tagging.tagURI(uri1, tags);

  // Add a second bookmark so that tags won't disappear when unchecked.
  let uri2 = Services.io.newURI("http://www2.mozilla.org/");
  let bm2 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "mozilla",
    url: uri2.spec,
  });
  PlacesUtils.tagging.tagURI(uri2, tags);

  let win = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    opening: TEST_URL,
    waitForStateStop: true,
  });

  registerCleanupFunction(async () => {
    bookmarkPanel.removeAttribute("animate");
    await BrowserTestUtils.closeWindow(win);
    await PlacesUtils.bookmarks.eraseEverything();
  });

  win.StarUI._createPanelIfNeeded();
  let bookmarkPanel = win.document.getElementById("editBookmarkPanel");
  bookmarkPanel.setAttribute("animate", false);
  let shownPromise = promisePopupShown(bookmarkPanel);

  let bookmarkStar = win.BookmarkingUI.star;
  bookmarkStar.click();

  await shownPromise;

  // Init panel.
  ok(win.gEditItemOverlay, "gEditItemOverlay is in context");
  ok(win.gEditItemOverlay.initialized, "gEditItemOverlay is initialized");

  await openTagSelector(win);
  let tagsSelector = win.document.getElementById("editBMPanel_tagsSelector");

  // Go by two so there is some untouched tag in the middle.
  for (let i = 8; i < tags.length; i += 2) {
    tagsSelector.selectedIndex = i;
    let listItem = tagsSelector.selectedItem;
    isnot(listItem, null, "Valid listItem found");

    tagsSelector.ensureElementIsVisible(listItem);
    let scrollTop = tagsSelector.scrollTop;

    ok(listItem.hasAttribute("checked"), "Item is checked " + i);
    let selectedTag = listItem.label;

    // Uncheck the tag.
    let promise = BrowserTestUtils.waitForEvent(
      tagsSelector,
      "BookmarkTagsSelectorUpdated"
    );
    EventUtils.synthesizeMouseAtCenter(listItem.firstElementChild, {}, win);
    await promise;
    is(scrollTop, tagsSelector.scrollTop, "Scroll position did not change");

    // The listbox is rebuilt, so we have to get the new element.
    let newItem = tagsSelector.selectedItem;
    isnot(newItem, null, "Valid new listItem found");
    ok(!newItem.hasAttribute("checked"), "New listItem is unchecked " + i);
    is(newItem.label, selectedTag, "Correct tag is still selected");

    // Check the tag.
    promise = BrowserTestUtils.waitForEvent(
      tagsSelector,
      "BookmarkTagsSelectorUpdated"
    );
    EventUtils.synthesizeMouseAtCenter(newItem.firstElementChild, {}, win);
    await promise;
    is(scrollTop, tagsSelector.scrollTop, "Scroll position did not change");
  }

  // Remove the second bookmark, then nuke some of the tags.
  await PlacesUtils.bookmarks.remove(bm2);

  // Allow the tag updates to complete
  await PlacesTestUtils.promiseAsyncUpdates();

  // Doing this backwords tests more interesting paths.
  for (let i = tags.length - 1; i >= 0; i -= 2) {
    tagsSelector.selectedIndex = i;
    let listItem = tagsSelector.selectedItem;
    isnot(listItem, null, "Valid listItem found");

    tagsSelector.ensureElementIsVisible(listItem);

    ok(listItem.hasAttribute("checked"), "Item is checked " + i);

    // Uncheck the tag.
    let promise = BrowserTestUtils.waitForEvent(
      tagsSelector,
      "BookmarkTagsSelectorUpdated"
    );
    EventUtils.synthesizeMouseAtCenter(listItem.firstElementChild, {}, win);
    await promise;
  }

  let hiddenPromise = promisePopupHidden(bookmarkPanel);
  let doneButton = win.document.getElementById("editBookmarkPanelDoneButton");
  doneButton.click();
  await hiddenPromise;
  // Cleanup.
  await PlacesUtils.bookmarks.remove(bm1);
});

function openTagSelector(win) {
  let promise = BrowserTestUtils.waitForEvent(
    win.document.getElementById("editBMPanel_tagsSelector"),
    "BookmarkTagsSelectorUpdated"
  );
  // Open the tags selector.
  win.document.getElementById("editBMPanel_tagsSelectorExpander").doCommand();
  return promise;
}
