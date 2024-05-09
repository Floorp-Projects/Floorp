/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(() => SpecialPowers.pushPrefEnv({ set: [["sidebar.revamp", true]] }));
registerCleanupFunction(() => SpecialPowers.popPrefEnv());

add_task(async function test_customize_sidebar_actions() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document } = win;
  const sidebar = document.getElementById("sidebar-main");
  ok(sidebar, "Sidebar is shown.");

  const button = sidebar.customizeButton;
  const promiseFocused = BrowserTestUtils.waitForEvent(win, "SidebarFocused");
  button.click();
  await promiseFocused;
  let customizeDocument = win.SidebarController.browser.contentDocument;
  const customizeComponent =
    customizeDocument.querySelector("sidebar-customize");
  let toolEntrypointsCount = sidebar.toolButtons.length;
  is(
    customizeComponent.toolInputs.length,
    toolEntrypointsCount,
    `${toolEntrypointsCount} inputs to toggle Firefox Tools are shown in the Customize Menu.`
  );
  for (const toolInput of customizeComponent.toolInputs) {
    toolInput.click();
    await BrowserTestUtils.waitForCondition(() => {
      let toggledTool = win.SidebarController.toolsAndExtensions.get(
        toolInput.name
      );
      return toggledTool.disabled;
    }, `The entrypoint for ${toolInput.name} has been disabled in the sidebar.`);
    toolEntrypointsCount = sidebar.toolButtons.length;
    is(
      toolEntrypointsCount,
      1,
      `The button for the ${toolInput.name} entrypoint has been removed.`
    );
    toolInput.click();
    await BrowserTestUtils.waitForCondition(() => {
      let toggledTool = win.SidebarController.toolsAndExtensions.get(
        toolInput.name
      );
      return !toggledTool.disabled;
    }, `The entrypoint for ${toolInput.name} has been re-enabled in the sidebar.`);
    toolEntrypointsCount = sidebar.toolButtons.length;
    is(
      toolEntrypointsCount,
      2,
      `The button for the ${toolInput.name} entrypoint has been added back.`
    );
    // Check ordering
    is(
      sidebar.toolButtons[1].getAttribute("view"),
      toolInput.name,
      `The button for the ${toolInput.name} entrypoint has been added back to the end of the list of tools/extensions entrypoints`
    );
  }

  await BrowserTestUtils.closeWindow(win);
});
