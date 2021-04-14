/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const SCALAR_NAME = "browser.ui.customized_widgets";
const BOOKMARKS_H2_2020_PREF = "browser.toolbars.bookmarks.2h2020";
const bookmarksInfo = [
  {
    title: "firefox",
    url: "http://example.com",
  },
  {
    title: "rules",
    url: "http://example.com/2",
  },
  {
    title: "yo",
    url: "http://example.com/2",
  },
];

// Setup.
add_task(async function test_bookmarks_toolbar_telemetry() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [BOOKMARKS_H2_2020_PREF, true],
      ["browser.toolbars.bookmarks.visibility", "newtab"],
    ],
  });

  // This is added during startup
  await TestUtils.waitForCondition(
    () =>
      keyedScalarExists(
        "browser.ui.toolbar_widgets",
        "bookmarks-bar_pinned_newtab",
        true
      ),
    `Waiting for "bookmarks-bar_pinned_newtab" to appear in Telemetry snapshot`
  );
  ok(true, `"bookmarks-bar_pinned_newtab"=true found in Telemetry`);

  await changeToolbarVisibilityViaContextMenu("never");
  await assertUIChange(
    "bookmarks-bar_move_newtab_never_toolbar-context-menu",
    1
  );

  await changeToolbarVisibilityViaContextMenu("newtab");
  await assertUIChange(
    "bookmarks-bar_move_never_newtab_toolbar-context-menu",
    1
  );

  await changeToolbarVisibilityViaContextMenu("always");
  await assertUIChange(
    "bookmarks-bar_move_newtab_always_toolbar-context-menu",
    1
  );

  // Extra windows are opened to make sure telemetry numbers aren't
  // double counted since there will be multiple instances of the
  // bookmarks toolbar.
  let extraWindows = [];
  extraWindows.push(await BrowserTestUtils.openNewBrowserWindow());

  Services.telemetry.getSnapshotForScalars("main", true);
  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: bookmarksInfo,
  });
  registerCleanupFunction(() => PlacesUtils.bookmarks.remove(bookmarks));
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "browser.engagement.bookmarks_toolbar_bookmark_added",
    3,
    "Bookmarks added value should be 3"
  );

  let newtab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  registerCleanupFunction(() => BrowserTestUtils.removeTab(newtab));
  let bookmarkToolbarButton = document.querySelector(
    "#PlacesToolbarItems > toolbarbutton"
  );
  bookmarkToolbarButton.click();
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "browser.engagement.bookmarks_toolbar_bookmark_opened",
    1,
    "Bookmarks opened value should be 1"
  );

  extraWindows.push(await BrowserTestUtils.openNewBrowserWindow());
  extraWindows.push(await BrowserTestUtils.openNewBrowserWindow());

  // Simulate dragging a bookmark within the toolbar to ensure
  // that the bookmarks_toolbar_bookmark_added probe doesn't increment
  let srcElement = document.querySelector("#PlacesToolbar .bookmark-item");
  let destElement = document.querySelector("#PlacesToolbar");
  await EventUtils.synthesizePlainDragAndDrop({
    srcElement,
    destElement,
  });

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "browser.engagement.bookmarks_toolbar_bookmark_added",
    3,
    "Bookmarks added value should still be 3"
  );

  for (let win of extraWindows) {
    await BrowserTestUtils.closeWindow(win);
  }
});

async function changeToolbarVisibilityViaContextMenu(nextState) {
  let contextMenu = document.querySelector("#toolbar-context-menu");
  let popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  let menuButton = document.getElementById("PanelUI-menu-button");
  EventUtils.synthesizeMouseAtCenter(
    menuButton,
    { type: "contextmenu" },
    window
  );
  await popupShown;
  let bookmarksToolbarMenu = document.querySelector("#toggle_PersonalToolbar");
  let subMenu = bookmarksToolbarMenu.querySelector("menupopup");
  popupShown = BrowserTestUtils.waitForEvent(subMenu, "popupshown");
  bookmarksToolbarMenu.openMenu(true);
  await popupShown;
  let menuItem = document.querySelector(
    `menuitem[data-visibility-enum="${nextState}"]`
  );
  subMenu.activateItem(menuItem);
  contextMenu.hidePopup();
}

async function assertUIChange(key, value) {
  await TestUtils.waitForCondition(
    () => keyedScalarExists(SCALAR_NAME, key, value),
    `Waiting for ${key} to appear in Telemetry snapshot`
  );
  ok(true, `${key}=${value} found in Telemetry`);
}

function keyedScalarExists(scalar, key, value) {
  let snapshot = Services.telemetry.getSnapshotForKeyedScalars("main", false)
    .parent;
  if (!snapshot.hasOwnProperty(scalar)) {
    return false;
  }
  if (!snapshot[scalar].hasOwnProperty(key)) {
    info(`Looking for ${key} in ${JSON.stringify(snapshot[scalar])}`);
    return false;
  }
  return snapshot[scalar][key] == value;
}
