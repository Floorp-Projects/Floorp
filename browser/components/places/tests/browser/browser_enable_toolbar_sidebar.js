/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test that the Bookmarks Toolbar and Sidebar can be enabled from the Bookmarks Menu ("View history,
 * saved bookmarks, and more" button.
 */

// Cleanup.
registerCleanupFunction(async () => {
  CustomizableUI.setToolbarVisibility("PersonalToolbar", false);
  SidebarUI.hide();
});

async function selectAppMenuView(buttonId, viewId) {
  let btn;
  await BrowserTestUtils.waitForCondition(() => {
    btn = document.getElementById(buttonId);
    return btn;
  }, "Should have the " + buttonId + "button");
  let view = document.getElementById(viewId);
  let viewPromise = BrowserTestUtils.waitForEvent(view, "ViewShown");
  btn.click();
  await viewPromise;
}

async function openBookmarkingToolsPanelInLibraryToolbarButton() {
  await selectAppMenuView("library-button", "appMenu-libraryView");
  await selectAppMenuView(
    "appMenu-library-bookmarks-button",
    "PanelUI-bookmarks"
  );
  await selectAppMenuView(
    "panelMenu_bookmarkingTools",
    "PanelUI-bookmarkingTools"
  );
}

add_task(async function test_enable_toolbar() {
  await openBookmarkingToolsPanelInLibraryToolbarButton();

  let viewBookmarksToolbarBtn;
  await BrowserTestUtils.waitForCondition(() => {
    viewBookmarksToolbarBtn = document.getElementById(
      "panelMenu_viewBookmarksToolbar"
    );
    return viewBookmarksToolbarBtn;
  }, "Should have the library 'View Bookmarks Toolbar' button.");
  viewBookmarksToolbarBtn.click();
  let toolbar;
  await BrowserTestUtils.waitForCondition(() => {
    toolbar = document.getElementById("PersonalToolbar");
    return !toolbar.collapsed;
  }, "Should have the Bookmarks Toolbar enabled.");
  Assert.ok(!toolbar.collapsed, "Bookmarks Toolbar is enabled");
});

add_task(async function test_enable_sidebar() {
  await openBookmarkingToolsPanelInLibraryToolbarButton();

  let viewBookmarksSidebarBtn;
  await BrowserTestUtils.waitForCondition(() => {
    viewBookmarksSidebarBtn = document.getElementById(
      "panelMenu_viewBookmarksSidebar"
    );
    return viewBookmarksSidebarBtn;
  }, "Should have the library 'View Bookmarks Sidebar' button.");
  viewBookmarksSidebarBtn.click();
  let sidebar;
  await BrowserTestUtils.waitForCondition(() => {
    sidebar = document.getElementById("sidebar-box");
    return !sidebar.hidden;
  }, "Should have the Bookmarks Sidebar enabled.");
  Assert.ok(!sidebar.hidden, "Bookmarks Sidebar is enabled");
});
