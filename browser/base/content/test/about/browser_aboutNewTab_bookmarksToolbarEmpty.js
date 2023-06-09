/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const bookmarksInfo = [
  {
    title: "firefox",
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    url: "http://example.com",
  },
  {
    title: "rules",
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    url: "http://example.com/2",
  },
  {
    title: "yo",
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    url: "http://example.com/2",
  },
];

async function emptyToolbarMessageVisible(visible, win = window) {
  info("Empty toolbar message should be " + (visible ? "visible" : "hidden"));
  let emptyMessage = win.document.getElementById("personal-toolbar-empty");
  await BrowserTestUtils.waitForMutationCondition(
    emptyMessage,
    { attributes: true, attributeFilter: ["hidden"] },
    () => emptyMessage.hidden != visible
  );
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    // Ensure we can wait for about:newtab to load.
    set: [["browser.newtab.preload", false]],
  });
  // Move all existing bookmarks in the Bookmarks Toolbar and
  // Other Bookmarks to the Bookmarks Menu so they don't affect
  // the visibility of the Bookmarks Toolbar. Restore them at
  // the end of the test.
  let Bookmarks = PlacesUtils.bookmarks;
  let toolbarBookmarks = [];
  let unfiledBookmarks = [];
  let guidBookmarkTuples = [
    [Bookmarks.toolbarGuid, toolbarBookmarks],
    [Bookmarks.unfiledGuid, unfiledBookmarks],
  ];
  for (let [parentGuid, arr] of guidBookmarkTuples) {
    await Bookmarks.fetch({ parentGuid }, bookmark => arr.push(bookmark));
  }
  await Promise.all(
    [...toolbarBookmarks, ...unfiledBookmarks].map(async bookmark => {
      bookmark.parentGuid = Bookmarks.menuGuid;
      return Bookmarks.update(bookmark);
    })
  );
  registerCleanupFunction(async () => {
    for (let [parentGuid, arr] of guidBookmarkTuples) {
      await Promise.all(
        arr.map(async bookmark => {
          bookmark.parentGuid = parentGuid;
          return Bookmarks.update(bookmark);
        })
      );
    }
  });
});

add_task(async function bookmarks_toolbar_not_shown_when_empty() {
  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: bookmarksInfo,
  });
  let exampleTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "https://example.com",
  });
  let newtab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:newtab",
  });
  let emptyMessage = document.getElementById("personal-toolbar-empty");

  // 1: Test that the toolbar is shown in a newly opened foreground about:newtab
  let placesItems = document.getElementById("PlacesToolbarItems");
  let promiseBookmarksOnToolbar = BrowserTestUtils.waitForMutationCondition(
    placesItems,
    { childList: true },
    () => placesItems.childNodes.length
  );
  await waitForBookmarksToolbarVisibility({
    visible: true,
    message: "Toolbar should be visible on newtab",
  });
  await promiseBookmarksOnToolbar;
  await emptyToolbarMessageVisible(false);

  // 2: Toolbar should get hidden when switching tab to example.com
  let promiseToolbar = waitForBookmarksToolbarVisibility({
    visible: false,
    message: "Toolbar should be hidden on example.com",
  });
  await BrowserTestUtils.switchTab(gBrowser, exampleTab);
  await promiseToolbar;

  // 3: Remove all children of the Bookmarks Toolbar and confirm that
  // the toolbar should not become visible when switching to newtab
  CustomizableUI.addWidgetToArea(
    "personal-bookmarks",
    CustomizableUI.AREA_TABSTRIP
  );
  CustomizableUI.removeWidgetFromArea("import-button");
  await BrowserTestUtils.switchTab(gBrowser, newtab);
  await waitForBookmarksToolbarVisibility({
    visible: true,
    message: "Toolbar is visible when there are no items in the toolbar area",
  });
  await emptyToolbarMessageVisible(true);

  // Click the link and check we open the library:
  let winPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
  EventUtils.synthesizeMouseAtCenter(
    emptyMessage.querySelector(".text-link"),
    {}
  );
  let libraryWin = await winPromise;
  is(
    libraryWin.document.location.href,
    "chrome://browser/content/places/places.xhtml",
    "Should have opened library."
  );
  await BrowserTestUtils.closeWindow(libraryWin);

  // 4: Put personal-bookmarks back in the toolbar and confirm the toolbar is visible now
  CustomizableUI.addWidgetToArea(
    "personal-bookmarks",
    CustomizableUI.AREA_BOOKMARKS
  );
  await BrowserTestUtils.switchTab(gBrowser, exampleTab);
  await BrowserTestUtils.switchTab(gBrowser, newtab);
  promiseBookmarksOnToolbar = BrowserTestUtils.waitForMutationCondition(
    placesItems,
    { childList: true },
    () => placesItems.childNodes.length
  );
  await waitForBookmarksToolbarVisibility({
    visible: true,
    message: "Toolbar should be visible with Bookmarks Toolbar Items restored",
  });
  await promiseBookmarksOnToolbar;
  await emptyToolbarMessageVisible(false);

  // 5: Remove all the bookmarks in the toolbar and confirm that the toolbar
  // is hidden on the New Tab now
  await PlacesUtils.bookmarks.remove(bookmarks);
  await BrowserTestUtils.switchTab(gBrowser, exampleTab);
  await BrowserTestUtils.switchTab(gBrowser, newtab);
  await waitForBookmarksToolbarVisibility({
    visible: true,
    message:
      "Toolbar is visible when there are no items or nested bookmarks in the toolbar area",
  });
  await emptyToolbarMessageVisible(true);

  // 6: Add a toolbarbutton and make sure that the toolbar appears when the button is visible
  CustomizableUI.addWidgetToArea(
    "characterencoding-button",
    CustomizableUI.AREA_BOOKMARKS
  );
  await BrowserTestUtils.switchTab(gBrowser, exampleTab);
  await BrowserTestUtils.switchTab(gBrowser, newtab);
  await waitForBookmarksToolbarVisibility({
    visible: true,
    message: "Toolbar is visible when there is a visible button in the toolbar",
  });
  await emptyToolbarMessageVisible(false);

  await BrowserTestUtils.removeTab(newtab);
  await BrowserTestUtils.removeTab(exampleTab);
  CustomizableUI.reset();
});
