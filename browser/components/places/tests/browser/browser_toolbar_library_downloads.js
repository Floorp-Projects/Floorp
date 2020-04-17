/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that downloads are available in the downloads subview.
 */

async function openDownloadsInLibraryToolbarButton() {
  let libraryBtn = document.getElementById("library-button");
  let libView = document.getElementById("appMenu-libraryView");
  let viewShownPromise = BrowserTestUtils.waitForEvent(libView, "ViewShown");
  libraryBtn.click();
  await viewShownPromise;

  let downloadsButton;
  await BrowserTestUtils.waitForCondition(() => {
    downloadsButton = document.getElementById(
      "appMenu-library-downloads-button"
    );
    return downloadsButton;
  }, "Should have the downloads bookmarks button");

  let DownloadsView = document.getElementById("PanelUI-downloads");
  let viewRecentPromise = BrowserTestUtils.waitForEvent(
    DownloadsView,
    "ViewShown"
  );
  info("clicking on downloads library button");
  downloadsButton.click();
  await viewRecentPromise;
}

async function closeLibraryMenu() {
  let libView = document.getElementById("appMenu-libraryView");
  let viewHiddenPromise = BrowserTestUtils.waitForEvent(libView, "ViewHiding");
  EventUtils.synthesizeKey("KEY_Escape", {}, window);
  await viewHiddenPromise;
}

async function testDownloadedItems() {
  let testURIs = ["http://ubuntu.org/", "http://google.com/"];
  let downloadsMenu = document.getElementById("panelMenu_downloadsMenu");
  let items = downloadsMenu.childNodes;

  Assert.ok(items, "Download items should exists");
  Assert.equal(items.length, testURIs.length);
  Assert.equal(
    items[0]._shell.download.source.url,
    testURIs[0],
    "Should have the expected URL"
  );
  Assert.equal(
    items[1]._shell.download.source.url,
    testURIs[1],
    "Should have the expected URL"
  );

  let shownItems = downloadsMenu.querySelectorAll("toolbarbutton");
  Assert.equal(shownItems.length, 2, "Both downloads are shown");
}

add_task(async function test_downloads() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://google.com",
      transition: PlacesUtils.history.TRANSITION_DOWNLOAD,
    },
    {
      uri: "http://ubuntu.org",
      transition: PlacesUtils.history.TRANSITION_DOWNLOAD,
    },
  ]);
  await openDownloadsInLibraryToolbarButton();

  testDownloadedItems();
  await closeLibraryMenu();
});
