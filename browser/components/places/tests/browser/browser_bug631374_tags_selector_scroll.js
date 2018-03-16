 /**
  * This test checks that editing tags doesn't scroll the tags selector
  * listbox to wrong positions.
  */

const TEST_URL = "about:buildconfig";

add_task(async function() {
  await PlacesUtils.bookmarks.eraseEverything();
  let tags = ["a", "b", "c", "d", "e", "f", "g",
              "h", "i", "l", "m", "n", "o", "p"];

  // Add a bookmark and tag it.
  let uri1 = Services.io.newURI(TEST_URL);
  let bm1 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "mozilla",
    url: uri1.spec
  });
  PlacesUtils.tagging.tagURI(uri1, tags);

  // Add a second bookmark so that tags won't disappear when unchecked.
  let uri2 = Services.io.newURI("http://www2.mozilla.org/");
  let bm2 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "mozilla",
    url: uri2.spec
  });
  PlacesUtils.tagging.tagURI(uri2, tags);

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_URL,
    waitForStateStop: true
  });

  registerCleanupFunction(async () => {
    bookmarkPanel.removeAttribute("animate");
    await BrowserTestUtils.removeTab(tab);
    await PlacesUtils.bookmarks.eraseEverything();
  });

  let bookmarkPanel = document.getElementById("editBookmarkPanel");
  bookmarkPanel.setAttribute("animate", false);
  let shownPromise = promisePopupShown(bookmarkPanel);

  let bookmarkStar = BookmarkingUI.star;
  bookmarkStar.click();

  await shownPromise;

  // Init panel.
  ok(gEditItemOverlay, "gEditItemOverlay is in context");
  ok(gEditItemOverlay.initialized, "gEditItemOverlay is initialized");

  await openTagSelector();
  let tagsSelector = document.getElementById("editBMPanel_tagsSelector");

  // Go by two so there is some untouched tag in the middle.
  for (let i = 8; i < tags.length; i += 2) {
    tagsSelector.selectedIndex = i;
    let listItem = tagsSelector.selectedItem;
    isnot(listItem, null, "Valid listItem found");

    tagsSelector.ensureElementIsVisible(listItem);
    let visibleIndex = tagsSelector.getIndexOfFirstVisibleRow();

    ok(listItem.checked, "Item is checked " + i);
    let selectedTag = listItem.label;

    // Uncheck the tag.
    let promiseNotification = PlacesTestUtils.waitForNotification(
      "onItemChanged", (id, property) => property == "tags");
    listItem.checked = false;
    await promiseNotification;
    is(visibleIndex, tagsSelector.getIndexOfFirstVisibleRow(),
       "Scroll position did not change");

    // The listbox is rebuilt, so we have to get the new element.
    let newItem = tagsSelector.selectedItem;
    isnot(newItem, null, "Valid new listItem found");
    ok(!newItem.checked, "New listItem is unchecked " + i);
    is(newItem.label, selectedTag, "Correct tag is still selected");

    // Check the tag.
    promiseNotification = PlacesTestUtils.waitForNotification(
      "onItemChanged", (id, property) => property == "tags");
    newItem.checked = true;
    await promiseNotification;
    is(visibleIndex, tagsSelector.getIndexOfFirstVisibleRow(),
       "Scroll position did not change");
  }

  // Remove the second bookmark, then nuke some of the tags.
  await PlacesUtils.bookmarks.remove(bm2);

  // Doing this backwords tests more interesting paths.
  for (let i = tags.length - 1; i >= 0 ; i -= 2) {
    tagsSelector.selectedIndex = i;
    let listItem = tagsSelector.selectedItem;
    isnot(listItem, null, "Valid listItem found");

    tagsSelector.ensureElementIsVisible(listItem);
    let firstVisibleTag = tags[tagsSelector.getIndexOfFirstVisibleRow()];

    ok(listItem.checked, "Item is checked " + i);

    // Uncheck the tag.
    let promiseNotification = PlacesTestUtils.waitForNotification(
      "onItemChanged", (id, property) => property == "tags");
    listItem.checked = false;
    await promiseNotification;

    // Ensure the first visible tag is still visible in the list.
    let firstVisibleIndex = tagsSelector.getIndexOfFirstVisibleRow();
    let lastVisibleIndex = firstVisibleIndex + tagsSelector.getNumberOfVisibleRows() - 1;
    let expectedTagIndex = tags.indexOf(firstVisibleTag);
    ok(expectedTagIndex >= firstVisibleIndex &&
       expectedTagIndex <= lastVisibleIndex,
       "Scroll position is correct");

    // The listbox is rebuilt, so we have to get the new element.
    let newItem = tagsSelector.selectedItem;
    isnot(newItem, null, "Valid new listItem found");
    ok(newItem.checked, "New listItem is checked " + i);
    is(tagsSelector.selectedItem.label,
       tags[Math.min(i + 1, tags.length - 2)],
       "The next tag is now selected");
  }

  let hiddenPromise = promisePopupHidden(bookmarkPanel);
  let doneButton = document.getElementById("editBookmarkPanelDoneButton");
  doneButton.click();
  await hiddenPromise;
  // Cleanup.
  await PlacesUtils.bookmarks.remove(bm1);
});

function openTagSelector() {
  // Wait for the tags selector to be open.
  let promise = new Promise(resolve => {
    let row = document.getElementById("editBMPanel_tagsSelectorRow");
    row.addEventListener("DOMAttrModified", function onAttrModified() {
      resolve();
    }, {once: true});
  });
  // Open the tags selector.
  document.getElementById("editBMPanel_tagsSelectorExpander").doCommand();
  return promise;
}
