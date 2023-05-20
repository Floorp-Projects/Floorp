/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

registerCleanupFunction(async function () {
  await resetCustomization();

  // Ensure sidebar is hidden after each test:
  if (!document.getElementById("sidebar-box").hidden) {
    SidebarUI.hide();
  }
});

var showSidebar = async function (win = window) {
  let button = win.document.getElementById("sidebar-button");
  let sidebarFocusedPromise = BrowserTestUtils.waitForEvent(
    win.document,
    "SidebarFocused"
  );
  EventUtils.synthesizeMouseAtCenter(button, {}, win);
  await sidebarFocusedPromise;
  ok(win.SidebarUI.isOpen, "Sidebar is opened");
  ok(button.hasAttribute("checked"), "Toolbar button is checked");
};

var hideSidebar = async function (win = window) {
  let button = win.document.getElementById("sidebar-button");
  EventUtils.synthesizeMouseAtCenter(button, {}, win);
  ok(!win.SidebarUI.isOpen, "Sidebar is closed");
  ok(!button.hasAttribute("checked"), "Toolbar button isn't checked");
};

// Check the sidebar widget shows the default items
add_task(async function () {
  CustomizableUI.addWidgetToArea("sidebar-button", "nav-bar");

  await showSidebar();
  is(SidebarUI.currentID, "viewBookmarksSidebar", "Default sidebar selected");
  await SidebarUI.show("viewHistorySidebar");

  await hideSidebar();
  await showSidebar();
  is(SidebarUI.currentID, "viewHistorySidebar", "Selected sidebar remembered");

  await hideSidebar();
  let otherWin = await BrowserTestUtils.openNewBrowserWindow();
  await showSidebar(otherWin);
  is(
    otherWin.SidebarUI.currentID,
    "viewHistorySidebar",
    "Selected sidebar remembered across windows"
  );
  await hideSidebar(otherWin);

  await BrowserTestUtils.closeWindow(otherWin);
});
