/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testPopup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.2h2020", true]],
  });

  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "http://example.com",
    title: "firefox",
  });
  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.remove(bookmark);
    Services.prefs.clearUserPref("browser.toolbars.bookmarks.visibility");
  });

  for (let state of ["always", "newtab"]) {
    info(`Testing with state set to '${state}'`);
    await SpecialPowers.pushPrefEnv({
      set: [["browser.toolbars.bookmarks.visibility", state]],
    });

    let newtab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: "about:newtab",
      waitForLoad: false,
    });

    let bookmarksToolbar = document.getElementById("PersonalToolbar");
    await TestUtils.waitForCondition(
      () => !bookmarksToolbar.collapsed,
      "Wait for toolbar to become visible"
    );
    ok(!bookmarksToolbar.collapsed, "Bookmarks toolbar should be visible");

    // 1. Right-click on a bookmark and check that the submenu is visible
    let bookmarkItem = bookmarksToolbar.querySelector(
      `.bookmark-item[label="firefox"]`
    );
    ok(bookmarkItem, "Got bookmark");
    let contextMenu = document.getElementById("placesContext");
    let popup = await openContextMenu(contextMenu, bookmarkItem);
    ok(
      !popup.target.querySelector("#toggle_PersonalToolbar").hidden,
      "Bookmarks toolbar submenu should appear on a .bookmark-item"
    );
    contextMenu.hidePopup();

    // 2. Right-click on the empty area and check that the submenu is visible
    popup = await openContextMenu(contextMenu, bookmarksToolbar);
    ok(
      !popup.target.querySelector("#toggle_PersonalToolbar").hidden,
      "Bookmarks toolbar submenu should appear on the empty part of the toolbar"
    );

    let bookmarksToolbarMenu = document.querySelector(
      "#toggle_PersonalToolbar"
    );
    let subMenu = bookmarksToolbarMenu.querySelector("menupopup");
    bookmarksToolbarMenu.openMenu(true);
    await BrowserTestUtils.waitForPopupEvent(subMenu, "shown");
    let menuitems = subMenu.querySelectorAll("menuitem");
    for (let menuitem of menuitems) {
      let expected = menuitem.dataset.visibilityEnum == state;
      is(
        menuitem.getAttribute("checked"),
        expected.toString(),
        `The corresponding menuitem, ${menuitem.dataset.visibilityEnum}, ${
          expected ? "should" : "shouldn't"
        } be checked if state=${state}`
      );
    }

    contextMenu.hidePopup();

    BrowserTestUtils.removeTab(newtab);
  }
});

function openContextMenu(contextMenu, target) {
  let popupPromise = BrowserTestUtils.waitForPopupEvent(contextMenu, "shown");
  EventUtils.synthesizeMouseAtCenter(target, {
    type: "contextmenu",
    button: 2,
  });
  return popupPromise;
}
