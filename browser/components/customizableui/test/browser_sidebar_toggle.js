/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

registerCleanupFunction(async function() {
  await resetCustomization();

  // Ensure sidebar is hidden after each test:
  if (!document.getElementById("sidebar-box").hidden) {
    SidebarUI.hide();
  }
});

var showSidebar = async function() {
  let button = document.getElementById("sidebar-button");
  let sidebarFocusedPromise = BrowserTestUtils.waitForEvent(document, "SidebarFocused");
  EventUtils.synthesizeMouseAtCenter(button, {});
  await sidebarFocusedPromise;
  ok(SidebarUI.isOpen, "Sidebar is opened");
  ok(button.hasAttribute("checked"), "Toolbar button is checked");
};

var hideSidebar = async function() {
  let button = document.getElementById("sidebar-button");
  EventUtils.synthesizeMouseAtCenter(button, {});
  ok(!SidebarUI.isOpen, "Sidebar is closed");
  ok(!button.hasAttribute("checked"), "Toolbar button isn't checked");
};

// Check the sidebar widget shows the default items
add_task(async function() {
  CustomizableUI.addWidgetToArea("sidebar-button", "nav-bar");

  await showSidebar();
  is(SidebarUI.currentID, "viewBookmarksSidebar", "Default sidebar selected");
  await SidebarUI.show("viewHistorySidebar");

  await hideSidebar();
  await showSidebar();
  is(SidebarUI.currentID, "viewHistorySidebar", "Selected sidebar remembered");
});
