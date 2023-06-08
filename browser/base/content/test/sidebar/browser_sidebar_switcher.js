registerCleanupFunction(() => {
  SidebarUI.hide();
});

/**
 * Helper function that opens a sidebar switcher panel popup menu
 * @returns Promise that resolves when the switcher panel popup is shown
 *          without any action from a user/test
 */
function showSwitcherPanelPromise() {
  return new Promise(resolve => {
    SidebarUI._switcherPanel.addEventListener(
      "popupshown",
      () => {
        resolve();
      },
      { once: true }
    );
    SidebarUI.showSwitcherPanel();
  });
}

/**
 * Helper function that waits for a sidebar switcher panel's "popupshown" event
 * @returns Promise which resolves when the popup menu is opened
 */
async function waitForSwitcherPopupShown() {
  return BrowserTestUtils.waitForEvent(SidebarUI._switcherPanel, "popupshown");
}

/**
 * Helper function that sends a mouse click to a specific menu item or a key event to a focused menu item of the sidebar switcher panel menu popup. Provide a querySelector parameter when a click behavior is needed and a key for a keyboard keydown event.
 * @param {String} [querySelector=null]  An HTML attribute of the menu item
 *                                       to be clicked
 * @param {String} [key=null]            Event.key to be synthesized on a focused
 *                                       menu item, i.e. "KEY_Enter" or " "
 * @returns Promise that resolves when both the menu popup is hidden and
 *          the sidebar itself is focused
 */
function toggleSwitcherButton(querySelector = null, key = null) {
  let sidebarPopup = document.querySelector("#sidebarMenu-popup");
  let hideSwitcherPanelPromise = Promise.all([
    BrowserTestUtils.waitForEvent(window, "SidebarFocused"),
    BrowserTestUtils.waitForEvent(sidebarPopup, "popuphidden"),
  ]);
  if (querySelector) {
    document.querySelector(querySelector).click();
  } else if (key) {
    EventUtils.synthesizeKey(key, {});
  }
  return hideSwitcherPanelPromise;
}

/**
 * Helper function to test a specific key event handling of sidebar menu popup
 * items used to access a specific sidebar
 * @param {String} key           Event.key to be tested on popup menu items
 * @param {String} sidebarTitle  Title of the sidebar that is to be activated
 *                               during the test (capitalized one word versions),
 *                               i.e. "History" or "Tabs"
 */
async function testSidebarMenuKeyToggle(key, sidebarTitle) {
  info(`Testing "${key}" key handling of sidebar menu popup items
  to access ${sidebarTitle} sidebar`);

  Assert.ok(SidebarUI.isOpen, "Sidebar is open");

  let sidebarSwitcher = document.querySelector("#sidebar-switcher-target");
  let sidebar = document.getElementById("sidebar");
  let searchBox = sidebar.firstElementChild;

  // If focus is on the search field (i.e. on the History sidebar),
  // or if the focus is on the awesomebar (bug 1835899),
  // move it to the switcher target:

  if (searchBox && searchBox.matches(":focus")) {
    EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true, repeat: 2 });
  } else if (!sidebarSwitcher.matches(":focus")) {
    sidebarSwitcher.focus();
  }

  Assert.equal(
    document.activeElement,
    sidebarSwitcher,
    "The sidebar switcher target button is focused"
  );
  Assert.ok(
    sidebarSwitcher.matches(":focus"),
    "The sidebar switcher target button is focused"
  );
  Assert.equal(
    SidebarUI._switcherPanel.state,
    "closed",
    "Sidebar menu popup is closed"
  );

  let promisePopupShown = waitForSwitcherPopupShown();

  // Activate sidebar switcher target to open its sidebar menu popup:
  EventUtils.synthesizeKey(key, {});

  await promisePopupShown;

  Assert.equal(
    SidebarUI._switcherPanel.state,
    "open",
    "Sidebar menu popup is open"
  );

  info("Testing keyboard navigation between sidebar menu popup controls");

  Assert.ok(
    document.getElementById("sidebar-switcher-bookmarks").matches(":focus"),
    "The 1st sidebar menu item (Bookmarks toolbarbutton) is focused"
  );
  Assert.equal(
    document.getElementById("sidebar-switcher-bookmarks"),
    document.commandDispatcher.focusedElement,
    "The 1st sidebar menu item (Bookmarks toolbarbutton) is focused"
  );

  // Move focus to the tested sidebar switcher option:
  EventUtils.synthesizeKey("KEY_Tab", {});
  EventUtils.synthesizeKey("KEY_ArrowDown", {});
  EventUtils.synthesizeKey("KEY_ArrowUp", {});

  Assert.ok(
    document.getElementById("sidebar-switcher-history").matches(":focus"),
    "The 2nd sidebar menu item (History toolbarbutton) is focused"
  );
  Assert.equal(
    document.getElementById("sidebar-switcher-history"),
    document.commandDispatcher.focusedElement,
    "The 2nd sidebar menu item (History toolbarbutton) is focused"
  );

  if (sidebarTitle === "Tabs") {
    EventUtils.synthesizeKey("KEY_ArrowDown", {});

    Assert.ok(
      document.getElementById("sidebar-switcher-tabs").matches(":focus"),
      "The 3rd sidebar menu item (Synced Tabs toolbarbutton) is focused"
    );
    Assert.equal(
      document.getElementById("sidebar-switcher-tabs"),
      document.commandDispatcher.focusedElement,
      "The 3rd sidebar menu item (Synced Tabs toolbarbutton) is focused"
    );
  }

  // Activate the tested sidebar switcher option to open the tested sidebar:
  await toggleSwitcherButton(/* querySelector = */ null, key);
  await SidebarUI.show(
    `view${sidebarTitle}Sidebar`
  ); /* i.e. "viewHistorySidebar" */

  info("Testing keyboard navigation when a sidebar menu popup is closed");

  Assert.equal(
    SidebarUI._switcherPanel.state,
    "closed",
    "Sidebar menu popup is closed"
  );
  // Test the sidebar panel is updated
  Assert.equal(
    SidebarUI._box.getAttribute("sidebarcommand"),
    `view${sidebarTitle}Sidebar` /* i.e. "viewHistorySidebar" */,
    `${sidebarTitle} sidebar loaded with "${key}" key`
  );
  Assert.equal(
    SidebarUI.currentID,
    `view${sidebarTitle}Sidebar` /* i.e. "viewHistorySidebar" */,
    `${sidebarTitle}'s current ID is updated to a target view`
  );
}

add_task(async function markup() {
  // If a sidebar is already open, close it.
  if (!document.getElementById("sidebar-box").hidden) {
    Assert.ok(
      false,
      "Unexpected sidebar found - a previous test failed to cleanup correctly"
    );
    SidebarUI.hide();
  }

  let sidebarPopup = document.querySelector("#sidebarMenu-popup");
  let sidebarSwitcher = document.querySelector("#sidebar-switcher-target");
  let sidebarTitle = document.querySelector("#sidebar-title");

  info("Test default markup of the sidebar switcher control");

  Assert.equal(
    sidebarSwitcher.tagName,
    "toolbarbutton",
    "Sidebar switcher target control is a toolbarbutton"
  );
  Assert.equal(
    sidebarSwitcher.children[1],
    sidebarTitle,
    "Sidebar switcher target control has a child label element"
  );
  Assert.equal(
    sidebarTitle.tagName,
    "label",
    "Sidebar switcher target control has a label element (that is expected to provide its accessible name"
  );
  Assert.equal(
    sidebarSwitcher.getAttribute("aria-expanded"),
    "false",
    "Sidebar switcher button is collapsed by default"
  );

  info("Test dynamic changes in the markup of the sidebar switcher control");

  await SidebarUI.show("viewBookmarksSidebar");
  await showSwitcherPanelPromise();

  Assert.equal(
    sidebarSwitcher.getAttribute("aria-expanded"),
    "true",
    "Sidebar switcher button is expanded when a sidebar menu is shown"
  );

  let waitForPopupHidden = BrowserTestUtils.waitForEvent(
    sidebarPopup,
    "popuphidden"
  );

  // Close on Escape anywhere
  EventUtils.synthesizeKey("KEY_Escape", {});
  await waitForPopupHidden;

  Assert.equal(
    sidebarSwitcher.getAttribute("aria-expanded"),
    "false",
    "Sidebar switcher button is collapsed when a sidebar menu is dismissed"
  );

  SidebarUI.hide();
});

add_task(async function keynav() {
  // If a sidebar is already open, close it.
  if (SidebarUI.isOpen) {
    Assert.ok(
      false,
      "Unexpected sidebar found - a previous test failed to cleanup correctly"
    );
    SidebarUI.hide();
  }

  await SidebarUI.show("viewBookmarksSidebar");

  await testSidebarMenuKeyToggle("KEY_Enter", "History");
  await testSidebarMenuKeyToggle(" ", "Tabs");

  SidebarUI.hide();
});

add_task(async function mouse() {
  // If a sidebar is already open, close it.
  if (!document.getElementById("sidebar-box").hidden) {
    Assert.ok(
      false,
      "Unexpected sidebar found - a previous test failed to cleanup correctly"
    );
    SidebarUI.hide();
  }

  let sidebar = document.querySelector("#sidebar-box");
  await SidebarUI.show("viewBookmarksSidebar");

  await showSwitcherPanelPromise();
  await toggleSwitcherButton("#sidebar-switcher-history");
  Assert.equal(
    sidebar.getAttribute("sidebarcommand"),
    "viewHistorySidebar",
    "History sidebar loaded"
  );

  await showSwitcherPanelPromise();
  await toggleSwitcherButton("#sidebar-switcher-tabs");
  Assert.equal(
    sidebar.getAttribute("sidebarcommand"),
    "viewTabsSidebar",
    "Tabs sidebar loaded"
  );

  await showSwitcherPanelPromise();
  await toggleSwitcherButton("#sidebar-switcher-bookmarks");
  Assert.equal(
    sidebar.getAttribute("sidebarcommand"),
    "viewBookmarksSidebar",
    "Bookmarks sidebar loaded"
  );
});
