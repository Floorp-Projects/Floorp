/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(() =>
  SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["layout.css.devPixelsPerPx", 1],
    ],
  })
);
registerCleanupFunction(() => SpecialPowers.popPrefEnv());

const extData2 = { ...extData };

async function sendMessage(extension, msg, data) {
  extension.sendMessage({ msg, data });
  await extension.awaitMessage("done");
}

add_task(async function test_extension_sidebar_actions() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");

  const extension = ExtensionTestUtils.loadExtension({ ...extData });
  await extension.startup();
  // TODO: Once `sidebar.revamp` is either enabled by default, or removed
  // entirely, this test should run in the current window, and it should only
  // await one "sidebar" message. Bug 1896421
  await extension.awaitMessage("sidebar");
  await extension.awaitMessage("sidebar");
  is(sidebar.extensionButtons.length, 1, "Extension is shown in the sidebar.");

  // Default icon and title matches.
  let button = sidebar.extensionButtons[0];
  let iconUrl = `moz-extension://${extension.uuid}/icon.png`;
  is(button.iconSrc, iconUrl, "Extension has the correct icon.");
  is(button.title, "Default Title", "Extension has the correct title.");

  // Icon can be updated.
  await sendMessage(extension, "set-icon", "updated-icon.png");
  await sidebar.updateComplete;
  iconUrl = `moz-extension://${extension.uuid}/updated-icon.png`;
  is(button.iconSrc, iconUrl, "Extension has updated icon.");

  // Title can be updated.
  await sendMessage(extension, "set-title", "Updated Title");
  await sidebar.updateComplete;
  is(button.title, "Updated Title", "Extension has updated title.");

  sidebar.expanded = true;
  await sidebar.updateComplete;
  ok(button.hasVisibleLabel, "Title is visible when sidebar is expanded.");

  sidebar.expanded = false;
  await sidebar.updateComplete;
  ok(!button.hasVisibleLabel, "Title is hidden when sidebar is collapsed.");

  // Panel can be updated.
  await sendMessage(extension, "set-panel", "1.html");
  const panelUrl = `moz-extension://${extension.uuid}/1.html`;
  await TestUtils.waitForCondition(() => {
    const browser = SidebarController.browser.contentDocument.getElementById(
      "webext-panels-browser"
    );
    return browser.currentURI.spec === panelUrl;
  }, "The new panel is visible.");

  await extension.unload();
  await sidebar.updateComplete;
  is(
    sidebar.extensionButtons.length,
    0,
    "Extension is removed from the sidebar."
  );
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_open_new_window_after_install() {
  const extension = ExtensionTestUtils.loadExtension({ ...extData });
  await extension.startup();
  await extension.awaitMessage("sidebar");

  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");
  await extension.awaitMessage("sidebar");

  is(
    sidebar.extensionButtons.length,
    1,
    "Extension is shown in new browser window."
  );

  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url: "about:addons" },
    async browser => {
      await BrowserTestUtils.synthesizeMouseAtCenter(
        "categories-box button[name=extension]",
        {},
        browser
      );
      const extensionToggle = await TestUtils.waitForCondition(
        () =>
          browser.contentDocument.querySelector(
            `addon-card[addon-id="${extension.id}"] moz-toggle`
          ),
        "Toggle button for extension is shown."
      );

      let promiseEvent = BrowserTestUtils.waitForEvent(
        win,
        "SidebarItemRemoved"
      );
      extensionToggle.click();
      await promiseEvent;
      await sidebar.updateComplete;
      is(sidebar.extensionButtons.length, 0, "The extension is disabled.");

      promiseEvent = BrowserTestUtils.waitForEvent(win, "SidebarItemAdded");
      extensionToggle.click();
      await promiseEvent;
      await sidebar.updateComplete;
      is(sidebar.extensionButtons.length, 1, "The extension is enabled.");
    }
  );

  await extension.unload();
  await sidebar.updateComplete;
  is(
    sidebar.extensionButtons.length,
    0,
    "Extension is removed from the sidebar."
  );
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_open_new_private_window_after_install() {
  const extension = ExtensionTestUtils.loadExtension({ ...extData });
  await extension.startup();
  await extension.awaitMessage("sidebar");

  const privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  const { document } = privateWin;
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");
  await TestUtils.waitForCondition(
    () => sidebar.extensionButtons,
    "Extensions container is shown."
  );
  is(
    sidebar.extensionButtons.length,
    0,
    "Extension is hidden in private browser window."
  );

  await extension.unload();
  await BrowserTestUtils.closeWindow(privateWin);
});

add_task(async function test_customize_sidebar_extensions() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");

  const extension = ExtensionTestUtils.loadExtension({ ...extData });
  await extension.startup();
  await extension.awaitMessage("sidebar");
  await extension.awaitMessage("sidebar");
  is(sidebar.extensionButtons.length, 1, "Extension is shown in the sidebar.");

  const button = sidebar.customizeButton;
  const promiseFocused = BrowserTestUtils.waitForEvent(win, "SidebarFocused");
  button.click();
  await promiseFocused;
  let customizeDocument = win.SidebarController.browser.contentDocument;
  const customizeComponent =
    customizeDocument.querySelector("sidebar-customize");
  let extensionEntrypointsCount = sidebar.extensionButtons.length;
  is(
    customizeComponent.extensionLinks.length,
    extensionEntrypointsCount,
    `${extensionEntrypointsCount} links to manage sidebar extensions are shown in the Customize Menu.`
  );

  // Default icon and title matches.
  const extensionLink = customizeComponent.extensionLinks[0];
  let iconUrl = `moz-extension://${extension.uuid}/icon.png`;
  let iconEl = extensionLink.closest(".extension-item").querySelector(".icon");
  is(iconEl.src, iconUrl, "Extension has the correct icon.");
  is(
    extensionLink.textContent.trim(),
    "Default Title",
    "Extension has the correct title."
  );

  // Test manage extension
  extensionLink.click();
  await TestUtils.waitForCondition(() => {
    let spec = win.gBrowser.selectedTab.linkedBrowser.documentURI.spec;
    return spec.startsWith("about:addons");
  }, "about:addons is the new opened tab");

  await extension.unload();
  await sidebar.updateComplete;
  is(
    sidebar.extensionButtons.length,
    0,
    "Extension is removed from the sidebar."
  );
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_extensions_keyboard_navigation() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");

  const extension = ExtensionTestUtils.loadExtension({ ...extData });
  await extension.startup();
  await extension.awaitMessage("sidebar");
  await extension.awaitMessage("sidebar");
  is(sidebar.extensionButtons.length, 1, "Extension is shown in the sidebar.");
  const extension2 = ExtensionTestUtils.loadExtension({ ...extData2 });
  await extension2.startup();
  await extension2.awaitMessage("sidebar");
  await extension2.awaitMessage("sidebar");
  is(
    sidebar.extensionButtons.length,
    2,
    "Two extensions are shown in the sidebar."
  );

  const button = sidebar.customizeButton;
  const promiseFocused = BrowserTestUtils.waitForEvent(win, "SidebarFocused");
  button.click();
  await promiseFocused;
  let customizeDocument = win.SidebarController.browser.contentDocument;
  const customizeComponent =
    customizeDocument.querySelector("sidebar-customize");
  let extensionEntrypointsCount = sidebar.extensionButtons.length;
  is(
    customizeComponent.extensionLinks.length,
    extensionEntrypointsCount,
    `${extensionEntrypointsCount} links to manage sidebar extensions are shown in the Customize Menu.`
  );

  customizeComponent.extensionLinks[0].focus();
  ok(
    isActiveElement(customizeComponent.extensionLinks[0]),
    "First extension link is focused."
  );

  info("Press Arrow Down key.");
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  ok(
    isActiveElement(customizeComponent.extensionLinks[1]),
    "Second extension link is focused."
  );

  info("Press Arrow Up key.");
  EventUtils.synthesizeKey("KEY_ArrowUp", {}, win);
  ok(
    isActiveElement(customizeComponent.extensionLinks[0]),
    "First extension link is focused."
  );

  info("Press Enter key.");
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  await TestUtils.waitForCondition(() => {
    let spec = win.gBrowser.selectedTab.linkedBrowser.documentURI.spec;
    return spec.startsWith("about:addons");
  }, "about:addons is the new opened tab");

  await extension.unload();
  await extension2.unload();
  await sidebar.updateComplete;
  is(
    sidebar.extensionButtons.length,
    0,
    "Extensions are removed from the sidebar."
  );
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_extension_sidebar_hidpi_icon() {
  await SpecialPowers.pushPrefEnv({
    set: [["layout.css.devPixelsPerPx", 2]],
  });

  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");

  const extension = ExtensionTestUtils.loadExtension({ ...extData });
  await extension.startup();
  await extension.awaitMessage("sidebar");
  await extension.awaitMessage("sidebar");

  const { iconSrc } = sidebar.extensionButtons[0];
  is(
    iconSrc,
    `moz-extension://${extension.uuid}/icon@2x.png`,
    "Extension has the correct icon for HiDPI displays."
  );

  await SpecialPowers.popPrefEnv();
  await extension.unload();
  await BrowserTestUtils.closeWindow(win);
});
