/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(() => SpecialPowers.pushPrefEnv({ set: [["sidebar.revamp", true]] }));
registerCleanupFunction(() => SpecialPowers.popPrefEnv());

const imageBuffer = imageBufferFromDataURI(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQImWNgYGBgAAAABQABh6FO1AAAAABJRU5ErkJggg=="
);

function imageBufferFromDataURI(encodedImageData) {
  const decodedImageData = atob(encodedImageData);
  return Uint8Array.from(decodedImageData, byte => byte.charCodeAt(0)).buffer;
}

/* global browser */
const extData = {
  manifest: {
    sidebar_action: {
      default_icon: "default.png",
      default_panel: "default.html",
      default_title: "Default Title",
    },
  },
  useAddonManager: "temporary",

  files: {
    "default.html": `
      <!DOCTYPE html>
      <html>
      <head><meta charset="utf-8"/>
      <script src="sidebar.js"></script>
      </head>
      <body>
      A Test Sidebar
      </body></html>
    `,
    "sidebar.js": function () {
      window.onload = () => {
        browser.test.sendMessage("sidebar");
      };
    },
    "1.html": `
      <!DOCTYPE html>
      <html>
      <head><meta charset="utf-8"/></head>
      <body>
      A Test Sidebar
      </body></html>
    `,
    "default.png": imageBuffer,
    "1.png": imageBuffer,
  },

  background() {
    browser.test.onMessage.addListener(async ({ msg, data }) => {
      switch (msg) {
        case "set-icon":
          await browser.sidebarAction.setIcon({ path: data });
          break;
        case "set-panel":
          await browser.sidebarAction.setPanel({ panel: data });
          break;
        case "set-title":
          await browser.sidebarAction.setTitle({ title: data });
          break;
      }
      browser.test.sendMessage("done");
    });
  },
};

async function sendMessage(extension, msg, data) {
  extension.sendMessage({ msg, data });
  await extension.awaitMessage("done");
}

add_task(async function test_extension_sidebar_actions() {
  // TODO: Once `sidebar.revamp` is either enabled by default, or removed
  // entirely, this test should run in the current window, and it should only
  // await one "sidebar" message.
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document } = win;
  const sidebar = document.getElementById("sidebar-main");
  ok(sidebar, "Sidebar is shown.");

  const extension = ExtensionTestUtils.loadExtension({ ...extData });
  await extension.startup();
  await extension.awaitMessage("sidebar");
  await extension.awaitMessage("sidebar");
  is(sidebar.extensionButtons.length, 1, "Extension is shown in the sidebar.");

  // Default icon and title matches.
  const button = sidebar.extensionButtons[0];
  let iconUrl = `moz-extension://${extension.uuid}/default.png`;
  is(
    button.style.getPropertyValue("--action-icon"),
    `image-set(url("${iconUrl}"), url("${iconUrl}") 2x)`,
    "Extension has the correct icon."
  );
  is(button.title, "Default Title", "Extension has the correct title.");

  // Icon can be updated.
  await sendMessage(extension, "set-icon", "1.png");
  await sidebar.updateComplete;
  iconUrl = `moz-extension://${extension.uuid}/1.png`;
  is(
    button.style.getPropertyValue("--action-icon"),
    `image-set(url("${iconUrl}"), url("${iconUrl}") 2x)`,
    "Extension has updated icon."
  );

  // Title can be updated.
  await sendMessage(extension, "set-title", "Updated Title");
  await sidebar.updateComplete;
  is(button.title, "Updated Title", "Extension has updated title.");

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
  const sidebar = document.getElementById("sidebar-main");
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
  const sidebar = document.getElementById("sidebar-main");
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
