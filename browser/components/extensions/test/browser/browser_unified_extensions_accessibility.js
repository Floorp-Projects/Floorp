/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadTestSubscript("head_unified_extensions.js");

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

  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    "https://example.org/"
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  await Promise.all([extension1.startup(), extension2.startup()]);

  // Open the extension panel.
  await openExtensionsPanel();

  let item = getUnifiedExtensionsItem(extension1.id);
  ok(item, `expected item for ${extension1.id}`);

  info("moving focus to first item in the unified extensions panel");
  let actionButton = item.querySelector(
    ".unified-extensions-item-action-button"
  );
  let focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
  EventUtils.synthesizeKey("VK_TAB", {});
  await focused;
  is(
    actionButton,
    document.activeElement,
    "expected action button of first extension item to be focused"
  );

  item = getUnifiedExtensionsItem(extension2.id);
  ok(item, `expected item for ${extension2.id}`);

  info("moving focus to second item in the unified extensions panel");
  actionButton = item.querySelector(".unified-extensions-item-action-button");
  focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
  EventUtils.synthesizeKey("KEY_ArrowDown", {});
  await focused;
  is(
    actionButton,
    document.activeElement,
    "expected action button of second extension item to be focused"
  );

  info("granting permission");
  const popupHidden = BrowserTestUtils.waitForEvent(
    document,
    "popuphidden",
    true
  );
  EventUtils.synthesizeKey(" ", {});
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
  await openExtensionsPanel();

  let item = getUnifiedExtensionsItem(extension1.id);
  ok(item, `expected item for ${extension1.id}`);

  let messageDeck = item.querySelector(".unified-extensions-item-message-deck");
  ok(messageDeck, "expected a message deck element");
  is(
    messageDeck.selectedIndex,
    gUnifiedExtensions.MESSAGE_DECK_INDEX_DEFAULT,
    "expected selected message in the deck to be the default message"
  );

  info("moving focus to first item in the unified extensions panel");
  let actionButton = item.querySelector(
    ".unified-extensions-item-action-button"
  );
  let focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
  EventUtils.synthesizeKey("VK_TAB", {});
  await focused;
  is(
    actionButton,
    document.activeElement,
    "expected action button of the first extension item to be focused"
  );
  is(
    messageDeck.selectedIndex,
    gUnifiedExtensions.MESSAGE_DECK_INDEX_HOVER,
    "expected selected message in the deck to be the hover message"
  );

  info(
    "moving focus to menu button of the first item in the unified extensions panel"
  );
  let menuButton = item.querySelector(".unified-extensions-item-menu-button");
  focused = BrowserTestUtils.waitForEvent(menuButton, "focus");
  ok(menuButton, "expected menu button");
  EventUtils.synthesizeKey("VK_TAB", {});
  await focused;
  is(
    menuButton,
    document.activeElement,
    "expected menu button in first extension item to be focused"
  );
  is(
    messageDeck.selectedIndex,
    gUnifiedExtensions.MESSAGE_DECK_INDEX_MENU_HOVER,
    "expected selected message in the deck to be the message when hovering the menu button"
  );

  info("opening menu of the first item");
  const contextMenu = document.getElementById(
    "unified-extensions-context-menu"
  );
  ok(contextMenu, "expected menu");
  const shown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeKey(" ", {});
  await shown;

  await closeChromeContextMenu(contextMenu.id, null);

  info("moving focus back to the action button of the first item");
  focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  await focused;
  is(
    messageDeck.selectedIndex,
    gUnifiedExtensions.MESSAGE_DECK_INDEX_HOVER,
    "expected selected message in the deck to be the hover message"
  );

  // Moving to the third extension directly because the second extension cannot
  // do anything on the current page and its action button is disabled. Note
  // that this third extension does not have a browser action but it has
  // "activeTab", which makes the extension "clickable". This allows us to
  // verify the focus/blur behavior of custom elments.
  info("moving focus to third item in the panel");
  item = getUnifiedExtensionsItem(extension3.id);
  ok(item, `expected item for ${extension3.id}`);
  actionButton = item.querySelector(".unified-extensions-item-action-button");
  ok(actionButton, `expected action button for ${extension3.id}`);
  messageDeck = item.querySelector(".unified-extensions-item-message-deck");
  ok(messageDeck, `expected message deck for ${extension3.id}`);
  is(
    messageDeck.selectedIndex,
    gUnifiedExtensions.MESSAGE_DECK_INDEX_DEFAULT,
    "expected selected message in the deck to be the default message"
  );
  // Now that we checked everything on this third extension, let's actually
  // focus it with the arrow down key.
  focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
  EventUtils.synthesizeKey("KEY_ArrowDown", {});
  await focused;
  is(
    actionButton,
    document.activeElement,
    "expected action button of the third extension item to be focused"
  );
  is(
    messageDeck.selectedIndex,
    gUnifiedExtensions.MESSAGE_DECK_INDEX_HOVER,
    "expected selected message in the deck to be the hover message"
  );

  info(
    "moving focus to menu button of the third item in the unified extensions panel"
  );
  menuButton = item.querySelector(".unified-extensions-item-menu-button");
  focused = BrowserTestUtils.waitForEvent(menuButton, "focus");
  ok(menuButton, "expected menu button");
  EventUtils.synthesizeKey("VK_TAB", {});
  await focused;
  is(
    menuButton,
    document.activeElement,
    "expected menu button in third extension item to be focused"
  );
  is(
    messageDeck.selectedIndex,
    gUnifiedExtensions.MESSAGE_DECK_INDEX_MENU_HOVER,
    "expected selected message in the deck to be the message when hovering the menu button"
  );

  info("moving focus back to the action button of the third item");
  focused = BrowserTestUtils.waitForEvent(actionButton, "focus");
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  await focused;
  is(
    messageDeck.selectedIndex,
    gUnifiedExtensions.MESSAGE_DECK_INDEX_HOVER,
    "expected selected message in the deck to be the hover message"
  );

  await closeExtensionsPanel();

  await extension1.unload();
  await extension2.unload();
  await extension3.unload();
});

add_task(async function test_open_panel_with_keyboard_navigation() {
  const { button, panel } = gUnifiedExtensions;
  ok(button, "expected button");
  ok(panel, "expected panel");

  const listView = getListView();
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
  EventUtils.synthesizeKey(" ", {});
  await viewShown;

  await closeExtensionsPanel();

  // Force focus on the unified extensions button again.
  forceFocusUnifiedExtensionsButton();

  // Use the "return" key to open the panel.
  viewShown = BrowserTestUtils.waitForEvent(listView, "ViewShown");
  EventUtils.synthesizeKey("KEY_Enter", {});
  await viewShown;

  await closeExtensionsPanel();
});
