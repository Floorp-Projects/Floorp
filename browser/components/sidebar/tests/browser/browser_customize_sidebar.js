/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(() => SpecialPowers.pushPrefEnv({ set: [["sidebar.revamp", true]] }));
registerCleanupFunction(() => SpecialPowers.popPrefEnv());

add_task(async function test_customize_sidebar_actions() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");

  const button = sidebar.customizeButton;
  const promiseFocused = BrowserTestUtils.waitForEvent(win, "SidebarFocused");
  button.click();
  await promiseFocused;
  let customizeDocument = win.SidebarController.browser.contentDocument;
  const customizeComponent =
    customizeDocument.querySelector("sidebar-customize");
  let toolEntrypointsCount = sidebar.toolButtons.length;
  let checkedInputs = Array.from(customizeComponent.toolInputs).filter(
    input => input.checked
  );
  is(
    checkedInputs.length,
    toolEntrypointsCount,
    `${toolEntrypointsCount} inputs to toggle Firefox Tools are shown in the Customize Menu.`
  );
  is(
    customizeComponent.toolInputs.length,
    3,
    "Three default tools are shown in the customize menu"
  );
  let bookmarksInput = Array.from(customizeComponent.toolInputs).find(
    input => input.name === "viewBookmarksSidebar"
  );
  ok(
    !bookmarksInput.checked,
    "The bookmarks input is unchecked initally as Bookmarks are disabled initially."
  );
  for (const toolInput of customizeComponent.toolInputs) {
    let toolDisabledInitialState = !toolInput.checked;
    toolInput.click();
    await BrowserTestUtils.waitForCondition(() => {
      let toggledTool = win.SidebarController.toolsAndExtensions.get(
        toolInput.name
      );
      return toggledTool.disabled === !toolDisabledInitialState;
    }, `The entrypoint for ${toolInput.name} has been ${toolDisabledInitialState ? "enabled" : "disabled"} in the sidebar.`);
    toolEntrypointsCount = sidebar.toolButtons.length;
    checkedInputs = Array.from(customizeComponent.toolInputs).filter(
      input => input.checked
    );
    is(
      toolEntrypointsCount,
      checkedInputs.length,
      `The button for the ${toolInput.name} entrypoint has been ${
        toolDisabledInitialState ? "added" : "removed"
      }.`
    );
    toolInput.click();
    await BrowserTestUtils.waitForCondition(() => {
      let toggledTool = win.SidebarController.toolsAndExtensions.get(
        toolInput.name
      );
      return toggledTool.disabled === toolDisabledInitialState;
    }, `The entrypoint for ${toolInput.name} has been ${toolDisabledInitialState ? "disabled" : "enabled"} in the sidebar.`);
    toolEntrypointsCount = sidebar.toolButtons.length;
    checkedInputs = Array.from(customizeComponent.toolInputs).filter(
      input => input.checked
    );
    is(
      toolEntrypointsCount,
      checkedInputs.length,
      `The button for the ${toolInput.name} entrypoint has been ${
        toolDisabledInitialState ? "removed" : "added"
      }.`
    );
    // Check ordering
    if (!toolDisabledInitialState) {
      is(
        sidebar.toolButtons[sidebar.toolButtons.length - 1].getAttribute(
          "view"
        ),
        toolInput.name,
        `The button for the ${toolInput.name} entrypoint has been added back to the end of the list of tools/extensions entrypoints`
      );
    }
  }

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_customize_not_added_in_menubar() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  const { document } = win;
  if (document.hasPendingL10nMutations) {
    await BrowserTestUtils.waitForEvent(document, "L10nMutationsFinished");
  }
  let sidebarsMenu = document.getElementById("viewSidebarMenu");
  let menuItems = sidebarsMenu.querySelectorAll("menuitem");
  ok(
    !Array.from(menuItems).find(menuitem =>
      menuitem.getAttribute("label").includes("Customize")
    ),
    "The View > Sidebars menu doesn't include any option for 'customize'."
  );

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_manage_preferences_navigation() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document, SidebarController } = win;
  const { contentWindow } = SidebarController.browser;
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");

  const button = sidebar.customizeButton;
  const promiseFocused = BrowserTestUtils.waitForEvent(win, "SidebarFocused");
  button.click();
  await promiseFocused;
  let customizeDocument = win.SidebarController.browser.contentDocument;
  const customizeComponent =
    customizeDocument.querySelector("sidebar-customize");
  let manageSettings =
    customizeComponent.shadowRoot.getElementById("manage-settings");

  EventUtils.synthesizeMouseAtCenter(manageSettings, {}, contentWindow);
  await BrowserTestUtils.waitForCondition(
    () =>
      win.gBrowser.selectedTab.linkedBrowser.currentURI.spec ==
      "about:preferences",
    "Navigated to about:preferences tab"
  );
  is(
    win.gBrowser.selectedTab.linkedBrowser.currentURI.spec,
    "about:preferences",
    "Manage Settings link navigates to about:preferences."
  );

  await BrowserTestUtils.closeWindow(win);
});
