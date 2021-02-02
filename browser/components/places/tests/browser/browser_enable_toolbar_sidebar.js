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
  btn.click();
  let view = document.getElementById(viewId);
  let viewPromise = BrowserTestUtils.waitForEvent(view, "ViewShown");
  await viewPromise;
}

async function openBookmarkingPanelInLibraryToolbarButton() {
  await selectAppMenuView("library-button", "appMenu-libraryView");
  await selectAppMenuView(
    "appMenu-library-bookmarks-button",
    "PanelUI-bookmarks"
  );
}

add_task(async function test_enable_toolbar() {
  await openBookmarkingPanelInLibraryToolbarButton();
  let toolbar = document.getElementById("PersonalToolbar");
  Assert.ok(toolbar.collapsed, "Bookmarks Toolbar is hidden");

  let viewBookmarksToolbarBtn;
  await BrowserTestUtils.waitForCondition(() => {
    viewBookmarksToolbarBtn = document.getElementById(
      "panelMenu_viewBookmarksToolbar"
    );
    return viewBookmarksToolbarBtn;
  }, "Should have the library 'View Bookmarks Toolbar' button.");
  viewBookmarksToolbarBtn.click();
  await BrowserTestUtils.waitForCondition(
    () => !toolbar.collapsed,
    "Should have the Bookmarks Toolbar enabled."
  );
  Assert.ok(!toolbar.collapsed, "Bookmarks Toolbar is enabled");
});
