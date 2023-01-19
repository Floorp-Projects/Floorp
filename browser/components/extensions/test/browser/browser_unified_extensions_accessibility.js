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

  BrowserTestUtils.loadURIString(
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
  let actionButton = item.querySelector(
    ".unified-extensions-item-action-button"
  );
  let focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
  EventUtils.synthesizeKey("VK_TAB", {}, win);
  await focused;
  is(
    actionButton,
    win.document.activeElement,
    "expected action button of first extension item to be focused"
  );

  item = getUnifiedExtensionsItem(win, extension2.id);
  ok(item, `expected item for ${extension2.id}`);

  info("moving focus to second item in the unified extensions panel");
  actionButton = item.querySelector(".unified-extensions-item-action-button");
  focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  await focused;
  is(
    actionButton,
    win.document.activeElement,
    "expected action button of second extension item to be focused"
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
      // activeTab and browser_action needed to enable the action button in mv2.
      permissions: ["activeTab"],
      browser_action: {},
    },
    useAddonManager: "temporary",
  });
  const extension2 = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "2",
    },
    useAddonManager: "temporary",
  });
  const extension3 = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      name: "3",
      // activeTab enables the action button without a browser action in mv3.
      permissions: ["activeTab"],
    },
    useAddonManager: "temporary",
  });

  await extension1.startup();
  await extension2.startup();
  await extension3.startup();

  // Open the extension panel.
  await openExtensionsPanel(win);

  let item = getUnifiedExtensionsItem(win, extension1.id);
  ok(item, `expected item for ${extension1.id}`);

  let messageDeck = item.querySelector(".unified-extensions-item-message-deck");
  ok(messageDeck, "expected a message deck element");
  is(
    messageDeck.selectedIndex,
    win.gUnifiedExtensions.MESSAGE_DECK_INDEX_DEFAULT,
    "expected selected message in the deck to be the default message"
  );

  info("moving focus to first item in the unified extensions panel");
  let actionButton = item.querySelector(
    ".unified-extensions-item-action-button"
  );
  let focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
  EventUtils.synthesizeKey("VK_TAB", {}, win);
  await focused;
  is(
    actionButton,
    win.document.activeElement,
    "expected action button of the first extension item to be focused"
  );
  is(
    messageDeck.selectedIndex,
    win.gUnifiedExtensions.MESSAGE_DECK_INDEX_HOVER,
    "expected selected message in the deck to be the hover message"
  );

  info(
    "moving focus to menu button of the first item in the unified extensions panel"
  );
  let menuButton = item.querySelector(".unified-extensions-item-menu-button");
  focused = BrowserTestUtils.waitForEvent(menuButton, "focus");
  ok(menuButton, "expected menu button");
  EventUtils.synthesizeKey("VK_TAB", {}, win);
  await focused;
  is(
    menuButton,
    win.document.activeElement,
    "expected menu button in first extension item to be focused"
  );
  is(
    messageDeck.selectedIndex,
    win.gUnifiedExtensions.MESSAGE_DECK_INDEX_MENU_HOVER,
    "expected selected message in the deck to be the message when hovering the menu button"
  );

  info("opening menu of the first item");
  const contextMenu = win.document.getElementById(
    "unified-extensions-context-menu"
  );
  ok(contextMenu, "expected menu");
  const shown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeKey(" ", {}, win);
  await shown;

  await closeChromeContextMenu(contextMenu.id, null, win);

  info("moving focus back to the action button of the first item");
  focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true }, win);
  await focused;
  is(
    messageDeck.selectedIndex,
    win.gUnifiedExtensions.MESSAGE_DECK_INDEX_HOVER,
    "expected selected message in the deck to be the hover message"
  );

  // Moving to the third extension directly because the second extension cannot
  // do anything on the current page and its action button is disabled. Note
  // that this third extension does not have a browser action but it has
  // "activeTab", which makes the extension "clickable". This allows us to
  // verify the focus/blur behavior of custom elments.
  info("moving focus to third item in the panel");
  item = getUnifiedExtensionsItem(win, extension3.id);
  ok(item, `expected item for ${extension3.id}`);
  actionButton = item.querySelector(".unified-extensions-item-action-button");
  ok(actionButton, `expected action button for ${extension3.id}`);
  messageDeck = item.querySelector(".unified-extensions-item-message-deck");
  ok(messageDeck, `expected message deck for ${extension3.id}`);
  is(
    messageDeck.selectedIndex,
    win.gUnifiedExtensions.MESSAGE_DECK_INDEX_DEFAULT,
    "expected selected message in the deck to be the default message"
  );
  // Now that we checked everything on this third extension, let's actually
  // focus it with the arrow down key.
  focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  await focused;
  is(
    actionButton,
    win.document.activeElement,
    "expected action button of the third extension item to be focused"
  );
  is(
    messageDeck.selectedIndex,
    win.gUnifiedExtensions.MESSAGE_DECK_INDEX_HOVER,
    "expected selected message in the deck to be the hover message"
  );

  info(
    "moving focus to menu button of the third item in the unified extensions panel"
  );
  menuButton = item.querySelector(".unified-extensions-item-menu-button");
  focused = BrowserTestUtils.waitForEvent(menuButton, "focus");
  ok(menuButton, "expected menu button");
  EventUtils.synthesizeKey("VK_TAB", {}, win);
  await focused;
  is(
    menuButton,
    win.document.activeElement,
    "expected menu button in third extension item to be focused"
  );
  is(
    messageDeck.selectedIndex,
    win.gUnifiedExtensions.MESSAGE_DECK_INDEX_MENU_HOVER,
    "expected selected message in the deck to be the message when hovering the menu button"
  );

  info("moving focus back to the action button of the third item");
  focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true }, win);
  await focused;
  is(
    messageDeck.selectedIndex,
    win.gUnifiedExtensions.MESSAGE_DECK_INDEX_HOVER,
    "expected selected message in the deck to be the hover message"
  );

  await closeExtensionsPanel(win);

  await extension1.unload();
  await extension2.unload();
  await extension3.unload();
});

add_task(async function test_open_panel_with_keyboard_navigation() {
  const { button, panel } = win.gUnifiedExtensions;
  ok(button, "expected button");
  ok(panel, "expected panel");

  const listView = getListView(win);
  ok(listView, "expected list view");

  // Force focus on the unified extensions button.
  const forceFocusUnifiedExtensionsButton = () => {
    button.setAttribute("tabindex", "-1");
    button.focus();
    button.removeAttribute("tabindex");
  };
  forceFocusUnifiedExtensionsButton();

  // Use the "space" key to open the panel.
  let viewShown = BrowserTestUtils.waitForEvent(listView, "ViewShown");
  EventUtils.synthesizeKey(" ", {}, win);
  await viewShown;

  await closeExtensionsPanel(win);

  // Force focus on the unified extensions button again.
  forceFocusUnifiedExtensionsButton();

  // Use the "return" key to open the panel.
  viewShown = BrowserTestUtils.waitForEvent(listView, "ViewShown");
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  await viewShown;

  await closeExtensionsPanel(win);
});
