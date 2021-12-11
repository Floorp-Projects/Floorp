registerCleanupFunction(() => {
  SidebarUI.hide();
});

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

function clickSwitcherButton(querySelector) {
  let sidebarPopup = document.querySelector("#sidebarMenu-popup");
  let switcherPromise = Promise.all([
    BrowserTestUtils.waitForEvent(window, "SidebarFocused"),
    BrowserTestUtils.waitForEvent(sidebarPopup, "popuphidden"),
  ]);
  document.querySelector(querySelector).click();
  return switcherPromise;
}

add_task(async function() {
  // If a sidebar is already open, close it.
  if (!document.getElementById("sidebar-box").hidden) {
    ok(
      false,
      "Unexpected sidebar found - a previous test failed to cleanup correctly"
    );
    SidebarUI.hide();
  }

  let sidebar = document.querySelector("#sidebar-box");
  await SidebarUI.show("viewBookmarksSidebar");

  await showSwitcherPanelPromise();
  await clickSwitcherButton("#sidebar-switcher-history");
  is(
    sidebar.getAttribute("sidebarcommand"),
    "viewHistorySidebar",
    "History sidebar loaded"
  );

  await showSwitcherPanelPromise();
  await clickSwitcherButton("#sidebar-switcher-tabs");
  is(
    sidebar.getAttribute("sidebarcommand"),
    "viewTabsSidebar",
    "Tabs sidebar loaded"
  );

  await showSwitcherPanelPromise();
  await clickSwitcherButton("#sidebar-switcher-bookmarks");
  is(
    sidebar.getAttribute("sidebarcommand"),
    "viewBookmarksSidebar",
    "Bookmarks sidebar loaded"
  );
});
