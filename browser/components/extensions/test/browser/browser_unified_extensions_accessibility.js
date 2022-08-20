/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadTestSubscript("head_unified_extensions.js");

let win;

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.manifestV3.enabled", true]],
  });

  // Only load a new window with the unified extensions feature enabled once to
  // speed up the execution of this test file.
  win = await promiseEnableUnifiedExtensions();

  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
  });
});

add_task(async function test_keyboard_navigation_activeScript() {
  const extension1 = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      name: "1",
      content_scripts: [
        {
          matches: ["*://*/*"],
          js: ["script.js"],
        },
      ],
    },
    files: {
      "script.js": () => {
        browser.test.fail("this script should NOT have been executed");
      },
    },
    useAddonManager: "temporary",
  });
  const extension2 = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      name: "2",
      content_scripts: [
        {
          matches: ["*://*/*"],
          js: ["script.js"],
        },
      ],
    },
    files: {
      "script.js": () => {
        browser.test.sendMessage("script executed");
      },
    },
    useAddonManager: "temporary",
  });

  BrowserTestUtils.loadURI(
    win.gBrowser.selectedBrowser,
    "https://example.org/"
  );
  await BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);

  await Promise.all([extension1.startup(), extension2.startup()]);

  // Open the extension panel.
  await openExtensionsPanel(win);

  let item = getUnifiedExtensionsItem(win, extension1.id);
  ok(item, `expected item for ${extension1.id}`);

  info("moving focus to first item in the unified extensions panel");
  let actionButton = item.querySelector(".unified-extensions-item-action");
  let focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
  EventUtils.synthesizeKey("VK_TAB", {}, win);
  await focused;
  is(
    actionButton,
    win.document.activeElement,
    "expected primary button of first extension item to be focused"
  );

  item = getUnifiedExtensionsItem(win, extension2.id);
  ok(item, `expected item for ${extension2.id}`);

  info("moving focus to second item in the unified extensions panel");
  actionButton = item.querySelector(".unified-extensions-item-action");
  focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  await focused;
  is(
    actionButton,
    win.document.activeElement,
    "expected primary button of second extension item to be focused"
  );

  info("granting permission");
  const popupHidden = BrowserTestUtils.waitForEvent(
    win.document,
    "popuphidden",
    true
  );
  EventUtils.synthesizeKey(" ", {}, win);
  await Promise.all([popupHidden, extension2.awaitMessage("script executed")]);

  await Promise.all([extension1.unload(), extension2.unload()]);
});

add_task(async function test_keyboard_navigation_opens_menu() {
  const extension1 = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "1",
    },
    useAddonManager: "temporary",
  });
  const extension2 = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "2",
    },
    useAddonManager: "temporary",
  });

  await extension1.startup();
  await extension2.startup();

  // Open the extension panel.
  await openExtensionsPanel(win);

  let item = getUnifiedExtensionsItem(win, extension1.id);
  ok(item, `expected item for ${extension1.id}`);

  info("moving focus to first item in the unified extensions panel");
  let actionButton = item.querySelector(".unified-extensions-item-action");
  let focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
  EventUtils.synthesizeKey("VK_TAB", {}, win);
  await focused;
  is(
    actionButton,
    win.document.activeElement,
    "expected primary button of the first extension item to be focused"
  );
  ok(
    !item.hasAttribute("secondary-button-hovered"),
    "expected no secondary-button-hovered attr on the item"
  );

  info("moving focus to first menu button in the unified extensions panel");
  const menuButton = item.querySelector(".unified-extensions-item-open-menu");
  focused = BrowserTestUtils.waitForEvent(menuButton, "focus");
  ok(menuButton, "expected 'open menu' button");
  EventUtils.synthesizeKey("KEY_Tab", {}, win);
  await focused;
  is(
    menuButton,
    win.document.activeElement,
    "expected menu button in first extension item to be focused"
  );
  is(
    item.getAttribute("secondary-button-hovered"),
    "true",
    "expected secondary-button-hovered attr on the item"
  );

  info("opening menu");
  const contextMenu = win.document.getElementById(
    "unified-extensions-context-menu"
  );
  ok(contextMenu, "expected menu");
  const shown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeKey(" ", {}, win);
  await shown;

  await closeChromeContextMenu(contextMenu.id, null, win);

  info("moving focus back to the primary (action) button");
  focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true }, win);
  await focused;
  ok(
    !item.hasAttribute("secondary-button-hovered"),
    "expected no secondary-button-hovered attr on the item"
  );

  await closeExtensionsPanel(win);

  await extension1.unload();
  await extension2.unload();
});
