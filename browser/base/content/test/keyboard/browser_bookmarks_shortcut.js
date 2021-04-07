/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test the behavior of keypress shortcuts for the bookmarks toolbar.
 */

// Test that the bookmarks toolbar's visibility is toggled using the bookmarks-shortcut.
add_task(async function testBookmarksToolbarShortcut() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.2h2020", true]],
  });

  let blankTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "example.com",
    waitForLoad: false,
  });

  info("Toggle toolbar visibility on");
  let toolbar = document.getElementById("PersonalToolbar");
  is(
    toolbar.getAttribute("collapsed"),
    "true",
    "Toolbar bar should already be collapsed"
  );

  EventUtils.synthesizeKey("b", { shiftKey: true, accelKey: true });
  toolbar = document.getElementById("PersonalToolbar");
  await BrowserTestUtils.waitForAttribute("collapsed", toolbar, "false");
  ok(true, "bookmarks toolbar is visible");

  await testIsBookmarksMenuItemStateChecked("always");

  info("Toggle toolbar visibility off");
  EventUtils.synthesizeKey("b", { shiftKey: true, accelKey: true });
  toolbar = document.getElementById("PersonalToolbar");
  await BrowserTestUtils.waitForAttribute("collapsed", toolbar, "true");
  ok(true, "bookmarks toolbar is not visible");

  await testIsBookmarksMenuItemStateChecked("never");

  await BrowserTestUtils.removeTab(blankTab);
});

// Test that the bookmarks library windows opens with the new keyboard shortcut.
add_task(async function testNewBookmarksLibraryShortcut() {
  let blankTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "example.com",
    waitForLoad: false,
  });

  info("Check that the bookmarks library windows opens.");
  let bookmarksLibraryOpened = promiseOpenBookmarksLibrary();

  await EventUtils.synthesizeKey("o", { shiftKey: true, accelKey: true });

  let win = await bookmarksLibraryOpened;

  ok(true, "Bookmarks library successfully opened.");

  win.close();

  await BrowserTestUtils.removeTab(blankTab);
});

/**
 * Tests whether or not the bookmarks' menuitem state is checked.
 */
async function testIsBookmarksMenuItemStateChecked(expected) {
  info("Test that the toolbar menuitem state is correct.");
  let contextMenu = document.getElementById("toolbar-context-menu");
  let target = document.getElementById("PanelUI-menu-button");

  await openContextMenu(contextMenu, target);

  let menuitems = ["always", "never", "newtab"].map(e =>
    document.querySelector(`menuitem[data-visibility-enum="${e}"]`)
  );

  let checkedItem = menuitems.filter(m => m.getAttribute("checked") == "true");
  is(checkedItem.length, 1, "should have only one menuitem checked");
  is(
    checkedItem[0].dataset.visibilityEnum,
    expected,
    `checked menuitem should be ${expected}`
  );

  for (let menuitem of menuitems) {
    if (menuitem.dataset.visibilityEnum == expected) {
      ok(!menuitem.hasAttribute("key"), "dont show shortcut on current state");
    } else {
      is(
        menuitem.hasAttribute("key"),
        menuitem.dataset.visibilityEnum != "newtab",
        "shortcut is on the menuitem opposite of the current state excluding newtab"
      );
    }
  }

  await closeContextMenu(contextMenu);
}

/**
 * Returns a promise for opening the bookmarks library.
 */
async function promiseOpenBookmarksLibrary() {
  return BrowserTestUtils.domWindowOpened(null, async win => {
    await BrowserTestUtils.waitForEvent(win, "load");
    await TestUtils.waitForCondition(
      () =>
        win.document.documentURI ===
        "chrome://browser/content/places/places.xhtml"
    );
    return true;
  });
}

/**
 * Helper for opening the context menu.
 */
async function openContextMenu(contextMenu, target) {
  info("Opening context menu.");
  EventUtils.synthesizeMouseAtCenter(target, {
    type: "contextmenu",
  });
  await BrowserTestUtils.waitForPopupEvent(contextMenu, "shown");
  let bookmarksToolbarMenu = document.querySelector("#toggle_PersonalToolbar");
  let subMenu = bookmarksToolbarMenu.querySelector("menupopup");
  EventUtils.synthesizeMouseAtCenter(bookmarksToolbarMenu, {});
  await BrowserTestUtils.waitForPopupEvent(subMenu, "shown");
}

/**
 * Helper for closing the context menu.
 */
async function closeContextMenu(contextMenu) {
  info("Closing context menu.");
  contextMenu.hidePopup();
  await BrowserTestUtils.waitForPopupEvent(contextMenu, "hidden");
}
