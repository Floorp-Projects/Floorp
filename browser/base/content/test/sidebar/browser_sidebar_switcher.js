registerCleanupFunction(() => {
  SidebarController.hide();
});

/**
 * Helper function that opens a sidebar switcher panel popup menu
 * @returns Promise that resolves when the switcher panel popup is shown
 *          without any action from a user/test
 */
function showSwitcherPanelPromise() {
  return new Promise(resolve => {
    SidebarController._switcherPanel.addEventListener(
      "popupshown",
      () => {
        resolve();
      },
      { once: true }
    );
    SidebarController.showSwitcherPanel();
  });
}

/**
 * Helper function that waits for a sidebar switcher panel's "popupshown" event
 * @returns Promise which resolves when the popup menu is opened
 */
async function waitForSwitcherPopupShown() {
  return BrowserTestUtils.waitForEvent(
    SidebarController._switcherPanel,
    "popupshown"
  );
}

/**
 * Helper function that sends a mouse click to a specific menu item or a key
 * event to a active menu item of the sidebar switcher menu popup. Provide a
 * querySelector parameter when a click behavior is needed.
 * @param {String} [querySelector=null]  An HTML attribute of the menu item
 *                                       to be clicked
 * @returns Promise that resolves when both the menu popup is hidden and
 *          the sidebar itself is focused
 */
function pickSwitcherMenuitem(querySelector = null) {
  let sidebarPopup = document.querySelector("#sidebarMenu-popup");
  let hideSwitcherPanelPromise = Promise.all([
    BrowserTestUtils.waitForEvent(window, "SidebarFocused"),
    BrowserTestUtils.waitForEvent(sidebarPopup, "popuphidden"),
  ]);
  if (querySelector) {
    document.querySelector(querySelector).click();
  } else {
    EventUtils.synthesizeKey("KEY_Enter", {});
  }
  return hideSwitcherPanelPromise;
}

/**
 * Helper function to test a key handling of sidebar menu popup items used to
 * access a specific sidebar
 * @param {String} key           Event.key to open the switcher menu popup
 * @param {String} sidebarTitle  Title of the sidebar that is to be activated
 *                               during the test (capitalized one word versions),
 *                               i.e. "History" or "Tabs"
 */
async function testSidebarMenuKeyToggle(key, sidebarTitle) {
  info(`Testing "${key}" key handling of sidebar menu popup items
  to access ${sidebarTitle} sidebar`);

  Assert.ok(SidebarController.isOpen, "Sidebar is open");

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
    SidebarController._switcherPanel.state,
    "closed",
    "Sidebar menu popup is closed"
  );

  let promisePopupShown = waitForSwitcherPopupShown();

  // Activate sidebar switcher target to open its sidebar menu popup:
  EventUtils.synthesizeKey(key, {});

  await promisePopupShown;

  Assert.equal(
    SidebarController._switcherPanel.state,
    "open",
    "Sidebar menu popup is open"
  );

  info("Testing keyboard navigation between sidebar menu popup controls");

  let arrowDown = async (menuitemId, msg) => {
    let menuItemActive = BrowserTestUtils.waitForEvent(
      SidebarController._switcherPanel,
      "DOMMenuItemActive"
    );
    EventUtils.synthesizeKey("KEY_ArrowDown", {});
    await menuItemActive;
    Assert.ok(
      document.getElementById(menuitemId).hasAttribute("_moz-menuactive"),
      msg
    );
  };

  // Move to the first sidebar switcher option:
  await arrowDown(
    "sidebar-switcher-bookmarks",
    "The 1st sidebar menu item (Bookmarks) is active"
  );

  // Move to the next sidebar switcher option:
  await arrowDown(
    "sidebar-switcher-history",
    "The 2nd sidebar menu item (History) is active"
  );

  if (sidebarTitle === "Tabs") {
    await arrowDown(
      "sidebar-switcher-tabs",
      "The 3rd sidebar menu item (Synced Tabs) is active"
    );
  }

  // Activate the tested sidebar switcher option to open the tested sidebar:
  let sidebarShown = BrowserTestUtils.waitForEvent(window, "SidebarShown");
  await pickSwitcherMenuitem(/* querySelector = */ null);
  await sidebarShown;

  info("Testing keyboard navigation when a sidebar menu popup is closed");

  Assert.equal(
    SidebarController._switcherPanel.state,
    "closed",
    "Sidebar menu popup is closed"
  );
  // Test the sidebar panel is updated
  Assert.equal(
    SidebarController._box.getAttribute("sidebarcommand"),
    `view${sidebarTitle}Sidebar` /* e.g. "viewHistorySidebar" */,
    `${sidebarTitle} sidebar loaded`
  );
  Assert.equal(
    SidebarController.currentID,
    `view${sidebarTitle}Sidebar` /* e.g. "viewHistorySidebar" */,
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
    SidebarController.hide();
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

  await SidebarController.show("viewBookmarksSidebar");
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

  SidebarController.hide();
});

add_task(async function keynav() {
  // If a sidebar is already open, close it.
  if (SidebarController.isOpen) {
    Assert.ok(
      false,
      "Unexpected sidebar found - a previous test failed to cleanup correctly"
    );
    SidebarController.hide();
  }

  await SidebarController.show("viewBookmarksSidebar");

  await testSidebarMenuKeyToggle("KEY_Enter", "History");
  await testSidebarMenuKeyToggle(" ", "Tabs");

  SidebarController.hide();
});

add_task(async function mouse() {
  // If a sidebar is already open, close it.
  if (!document.getElementById("sidebar-box").hidden) {
    Assert.ok(
      false,
      "Unexpected sidebar found - a previous test failed to cleanup correctly"
    );
    SidebarController.hide();
  }

  let sidebar = document.querySelector("#sidebar-box");
  await SidebarController.show("viewBookmarksSidebar");

  await showSwitcherPanelPromise();
  await pickSwitcherMenuitem("#sidebar-switcher-history");
  Assert.equal(
    sidebar.getAttribute("sidebarcommand"),
    "viewHistorySidebar",
    "History sidebar loaded"
  );

  await showSwitcherPanelPromise();
  await pickSwitcherMenuitem("#sidebar-switcher-tabs");
  Assert.equal(
    sidebar.getAttribute("sidebarcommand"),
    "viewTabsSidebar",
    "Tabs sidebar loaded"
  );

  await showSwitcherPanelPromise();
  await pickSwitcherMenuitem("#sidebar-switcher-bookmarks");
  Assert.equal(
    sidebar.getAttribute("sidebarcommand"),
    "viewBookmarksSidebar",
    "Bookmarks sidebar loaded"
  );
});
