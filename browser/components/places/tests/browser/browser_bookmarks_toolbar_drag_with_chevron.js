/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.visibility", "newtab"]],
  });

  info("Open new tab and bookmarks toolbar");
  let { newTab } = await openNewTabAndBookmarksToolbar();

  info("Add bookmarks until showing chevron menu");
  let chevronMenu = document.getElementById("PlacesChevron");
  let bookmarkCount = 2;
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
  for (let max = bookmarkCount + 2; bookmarkCount < max; bookmarkCount++) {
    let url = `https://example.com/${bookmarkCount}`;
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url,
      title: url,
    });
  }

  BrowserTestUtils.removeTab(newTab);

  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
  });
});

add_task(async function drop_on_tabbar_from_chevron() {
  info("Open new tab and bookmarks toolbar");
  let { newTab, bookmarksToolbar } = await openNewTabAndBookmarksToolbar();

  info("Check whether DnD from chevron works");
  let chevronMenu = document.getElementById("PlacesChevron");
  await testForDndFromChevron(chevronMenu);

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
  await testForDndFromChevron(chevronMenu);

  BrowserTestUtils.removeTab(newTab);
  BrowserTestUtils.removeTab(contentTab);
});

add_task(async function drop_on_chevron_from_identity_box() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.visibility", "always"]],
  });

  const TEST_URL = "https://example.com/404_not_found";

  info("Open new tab and bookmarks toolbar");
  let { newTab } = await openNewTabAndBookmarksToolbar({
    url: TEST_URL,
  });

  info("Start DnD");
  let chevronMenu = document.getElementById("PlacesChevron");
  let identityBox = document.getElementById("identity-box");
  let chevronPopup = document.getElementById("PlacesChevronPopup");

  let onChevronPopupShown = BrowserTestUtils.waitForPopupEvent(
    chevronPopup,
    "shown"
  );
  await EventUtils.synthesizePlainDragAndDrop({
    srcElement: identityBox,
    destElement: chevronMenu,
  });
  await onChevronPopupShown;

  info("Check the last bookmark item");
  await BrowserTestUtils.waitForCondition(() => {
    let items = [...chevronPopup.children];
    let lastElement = items.findLast(
      c => c.nodeName == "menuitem" && BrowserTestUtils.isVisible(c)
    );
    return lastElement.label == "404 Not Found";
  });
  Assert.ok(true, "The current page is bookmarked with correct position");

  chevronPopup.hidePopup();
  BrowserTestUtils.removeTab(newTab);
  await SpecialPowers.popPrefEnv();
});

async function testForDndFromChevron(chevronMenu) {
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

async function openNewTabAndBookmarksToolbar({ url } = {}) {
  let newTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: url ?? "about:newtab",
    waitForLoad: !!url,
  });

  let bookmarksToolbar = document.getElementById("PersonalToolbar");
  await TestUtils.waitForCondition(
    () => !bookmarksToolbar.collapsed,
    "Wait for toolbar to become visible"
  );

  return { newTab, bookmarksToolbar };
}
