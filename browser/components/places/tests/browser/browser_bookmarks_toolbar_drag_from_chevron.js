/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.visibility", "newtab"]],
  });

  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
  });
});

add_task(async function drop_on_tabbar() {
  let bookmarksToolbar = document.getElementById("PersonalToolbar");
  let chevronMenu = document.getElementById("PlacesChevron");

  info("Open about:newtab to show the bookmark toolbar");
  let newTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:newtab",
    waitForLoad: false,
  });
  await TestUtils.waitForCondition(
    () => !bookmarksToolbar.collapsed,
    "Wait for toolbar to become visible"
  );

  info("Add bookmarks until showing chevron menu");
  let bookmarkCount = 1;
  for (; ; bookmarkCount++) {
    let url = `https://example.com/${bookmarkCount}`;
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url,
      title: url,
    });

    if (BrowserTestUtils.isVisible(chevronMenu)) {
      break;
    }
  }

  info("Add more bookmarks");
  bookmarkCount += 1;
  for (let max = bookmarkCount + 5; bookmarkCount < max; bookmarkCount++) {
    let url = `https://example.com/${bookmarkCount}`;
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url,
      title: url,
    });
  }

  info("Check whether DnD from chevron works");
  await dndTest(chevronMenu);

  info("Open not about:newtab page to collapse the bookmark toolbar");
  let contentTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "https://example.com/",
    waitForLoad: false,
  });
  await BrowserTestUtils.waitForCondition(
    () => bookmarksToolbar.collapsed,
    "Wait for toolbar to become invisible"
  );

  info("Select about:newtab tab again");
  gBrowser.selectedTab = newTab;
  await BrowserTestUtils.waitForCondition(
    () => !bookmarksToolbar.collapsed,
    "Wait for toolbar to become visible"
  );

  info("Check whether DnD from chevron works again");
  await dndTest(chevronMenu);

  BrowserTestUtils.removeTab(newTab);
  BrowserTestUtils.removeTab(contentTab);
});

async function dndTest(chevronMenu) {
  info("Open chevron menu");
  let chevronPopup = document.getElementById("PlacesChevronPopup");
  let onChevronPopupShown = BrowserTestUtils.waitForPopupEvent(
    chevronPopup,
    "shown"
  );
  chevronMenu.click();
  await onChevronPopupShown;

  info("Choose a menu item from chevron menu as drag target");
  let srcElement = await BrowserTestUtils.waitForCondition(() =>
    [...chevronPopup.children].findLast(
      c =>
        c.label?.startsWith("https://example.com/") &&
        BrowserTestUtils.isVisible(c)
    )
  );

  let destElement = document.getElementById(
    gBrowser.tabContainer.hasAttribute("overflow")
      ? "new-tab-button"
      : "tabs-newtab-button"
  );
  let onNewTabByDnD = BrowserTestUtils.waitForNewTab(
    gBrowser,
    srcElement.label
  );

  info("Start DnD");
  await EventUtils.synthesizePlainDragAndDrop({
    srcElement,
    destElement,
  });

  info("Wait until finishing DnD");
  let dropTab = await onNewTabByDnD;
  Assert.ok(dropTab, "DnD was successful");
  BrowserTestUtils.removeTab(dropTab);
  chevronPopup.hidePopup();
}
