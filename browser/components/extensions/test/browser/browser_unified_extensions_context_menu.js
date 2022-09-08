/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

ChromeUtils.defineModuleGetter(
  this,
  "AbuseReporter",
  "resource://gre/modules/AbuseReporter.jsm"
);

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const TELEMETRY_EVENTS_FILTERS = {
  category: "addonsManager",
  method: "action",
};

loadTestSubscript("head_unified_extensions.js");

// We expect this rejection when the abuse report dialog window is
// being forcefully closed as part of the related test task.
PromiseTestUtils.allowMatchingRejectionsGlobally(/report dialog closed/);

const promiseExtensionUninstalled = extensionId => {
  return new Promise(resolve => {
    let listener = {};
    listener.onUninstalled = addon => {
      if (addon.id == extensionId) {
        AddonManager.removeAddonListener(listener);
        resolve();
      }
    };
    AddonManager.addAddonListener(listener);
  });
};

function waitClosedWindow(win) {
  return new Promise(resolve => {
    function onWindowClosed() {
      if (win && !win.closed) {
        // If a specific window reference has been passed, then check
        // that the window is closed before resolving the promise.
        return;
      }
      Services.obs.removeObserver(onWindowClosed, "xul-window-destroyed");
      resolve();
    }
    Services.obs.addObserver(onWindowClosed, "xul-window-destroyed");
  });
}

let win;

add_setup(async function() {
  // Only load a new window with the unified extensions feature enabled once to
  // speed up the execution of this test file.
  win = await promiseEnableUnifiedExtensions();

  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
  });
});

add_task(async function test_context_menu() {
  const [extension] = createExtensions([{ name: "an extension" }]);
  await extension.startup();

  // Open the extension panel.
  await openExtensionsPanel(win);

  // Get the menu button of the extension and verify the mouseover/mouseout
  // behavior. We expect a help message to be displayed when a
  // `secondary-button-hovered` attribute is set to `true` on the item
  // (combined with some CSS).
  const item = getUnifiedExtensionsItem(win, extension.id);
  ok(item, "expected an item for the extension");
  const menuButton = item.querySelector(".unified-extensions-item-open-menu");
  ok(menuButton, "expected 'open menu' button");

  let messageHover = item.querySelector(
    ".unified-extensions-item-message-hover"
  );

  ok(
    BrowserTestUtils.is_visible(
      item.querySelector(".unified-extensions-item-message-default")
    ),
    "expected message to be visible"
  );
  ok(
    BrowserTestUtils.is_hidden(messageHover),
    "expected 'hover message' to be hidden"
  );

  let hovered = BrowserTestUtils.waitForEvent(menuButton, "mouseover");
  EventUtils.synthesizeMouseAtCenter(menuButton, { type: "mouseover" }, win);
  await hovered;
  is(
    item.getAttribute("secondary-button-hovered"),
    "true",
    "expected secondary-button-hovered attr on the item"
  );
  ok(
    BrowserTestUtils.is_hidden(
      item.querySelector(".unified-extensions-item-message-default")
    ),
    "expected message to be hidden"
  );
  ok(
    BrowserTestUtils.is_visible(messageHover),
    "expected 'hover message' to be visible"
  );
  Assert.deepEqual(
    win.document.l10n.getAttributes(messageHover),
    { id: "unified-extensions-item-message-manage", args: null },
    "expected correct l10n attributes for the message displayed on secondary button hovered"
  );

  let notHovered = BrowserTestUtils.waitForEvent(menuButton, "mouseout");
  // Move mouse somewhere else...
  EventUtils.synthesizeMouseAtCenter(item, { type: "mouseover" }, win);
  await notHovered;
  ok(
    !item.hasAttribute("secondary-button-hovered"),
    "expected no secondary-button-hovered attr on the item"
  );
  ok(
    BrowserTestUtils.is_visible(
      item.querySelector(".unified-extensions-item-message-default")
    ),
    "expected message to be visible"
  );
  ok(
    BrowserTestUtils.is_hidden(messageHover),
    "expected 'hover message' to be hidden"
  );

  // Open the context menu for the extension.
  const contextMenu = await openUnifiedExtensionsContextMenu(win, extension.id);
  const doc = contextMenu.ownerDocument;

  const manageButton = contextMenu.querySelector(
    ".unified-extensions-context-menu-manage-extension"
  );
  ok(manageButton, "expected manage button");
  is(manageButton.hidden, false, "expected manage button to be visible");
  is(manageButton.disabled, false, "expected manage button to be enabled");
  Assert.deepEqual(
    doc.l10n.getAttributes(manageButton),
    { id: "unified-extensions-context-menu-manage-extension", args: null },
    "expected correct l10n attributes for manage button"
  );

  const removeButton = contextMenu.querySelector(
    ".unified-extensions-context-menu-remove-extension"
  );
  ok(removeButton, "expected remove button");
  is(removeButton.hidden, false, "expected remove button to be visible");
  is(removeButton.disabled, false, "expected remove button to be enabled");
  Assert.deepEqual(
    doc.l10n.getAttributes(removeButton),
    { id: "unified-extensions-context-menu-remove-extension", args: null },
    "expected correct l10n attributes for remove button"
  );

  const reportButton = contextMenu.querySelector(
    ".unified-extensions-context-menu-report-extension"
  );
  ok(reportButton, "expected report button");
  is(reportButton.hidden, false, "expected report button to be visible");
  is(reportButton.disabled, false, "expected report button to be enabled");
  Assert.deepEqual(
    doc.l10n.getAttributes(reportButton),
    { id: "unified-extensions-context-menu-report-extension", args: null },
    "expected correct l10n attributes for report button"
  );

  await closeChromeContextMenu(contextMenu.id, null, win);
  await closeExtensionsPanel(win);

  await extension.unload();
});

add_task(
  async function test_context_menu_report_button_hidden_when_abuse_report_disabled() {
    await SpecialPowers.pushPrefEnv({
      set: [["extensions.abuseReport.enabled", false]],
    });

    const [extension] = createExtensions([{ name: "an extension" }]);
    await extension.startup();

    // Open the extension panel, then open the contextMenu for the extension.
    await openExtensionsPanel(win);
    const contextMenu = await openUnifiedExtensionsContextMenu(
      win,
      extension.id
    );

    const reportButton = contextMenu.querySelector(
      ".unified-extensions-context-menu-report-extension"
    );
    ok(reportButton, "expected report button");
    is(reportButton.hidden, true, "expected report button to be hidden");

    await closeChromeContextMenu(contextMenu.id, null, win);
    await closeExtensionsPanel(win);

    await extension.unload();
  }
);

add_task(
  async function test_context_menu_remove_button_disabled_when_extension_cannot_be_uninstalled() {
    const [extension] = createExtensions([{ name: "an extension" }]);
    await extension.startup();

    await EnterprisePolicyTesting.setupPolicyEngineWithJson({
      policies: {
        Extensions: {
          Locked: [extension.id],
        },
      },
    });

    // Open the extension panel, then open the context menu for the extension.
    await openExtensionsPanel(win);
    const contextMenu = await openUnifiedExtensionsContextMenu(
      win,
      extension.id
    );

    const removeButton = contextMenu.querySelector(
      ".unified-extensions-context-menu-remove-extension"
    );
    ok(removeButton, "expected remove button");
    is(removeButton.disabled, true, "expected remove button to be disabled");

    await closeChromeContextMenu(contextMenu.id, null, win);
    await closeExtensionsPanel(win);

    await extension.unload();
    await EnterprisePolicyTesting.setupPolicyEngineWithJson("");
  }
);

add_task(async function test_manage_extension() {
  Services.telemetry.clearEvents();

  // Navigate away from the initial page so that about:addons always opens in a
  // new tab during tests.
  BrowserTestUtils.loadURI(win.gBrowser.selectedBrowser, "about:robots");
  await BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);

  const [extension] = createExtensions([{ name: "an extension" }]);
  await extension.startup();

  // Open the extension panel, then open the context menu for the extension.
  await openExtensionsPanel(win);
  const contextMenu = await openUnifiedExtensionsContextMenu(win, extension.id);

  const manageButton = contextMenu.querySelector(
    ".unified-extensions-context-menu-manage-extension"
  );
  ok(manageButton, "expected manage button");

  // Click the "manage extension" context menu item, and wait until the menu is
  // closed and about:addons is open.
  const hidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  const aboutAddons = BrowserTestUtils.waitForNewTab(
    win.gBrowser,
    "about:addons",
    true
  );
  contextMenu.activateItem(manageButton);
  const [aboutAddonsTab] = await Promise.all([aboutAddons, hidden]);

  // Close the tab containing about:addons because we don't need it anymore.
  BrowserTestUtils.removeTab(aboutAddonsTab);

  await extension.unload();

  TelemetryTestUtils.assertEvents(
    [
      {
        object: "unifiedExtensions",
        value: null,
        extra: { addonId: extension.id, action: "manage" },
      },
    ],
    TELEMETRY_EVENTS_FILTERS
  );
});

add_task(async function test_report_extension() {
  SpecialPowers.pushPrefEnv({
    set: [["extensions.abuseReport.enabled", true]],
  });

  const [extension] = createExtensions([{ name: "an extension" }]);
  await extension.startup();

  await BrowserTestUtils.withNewTab({ gBrowser: win.gBrowser }, async () => {
    // Open the extension panel, then open the context menu for the extension.
    await openExtensionsPanel(win);
    const contextMenu = await openUnifiedExtensionsContextMenu(
      win,
      extension.id
    );

    const reportButton = contextMenu.querySelector(
      ".unified-extensions-context-menu-report-extension"
    );
    ok(reportButton, "expected report button");

    // Click the "report extension" context menu item, and wait until the menu is
    // closed and about:addons is open with the "abuse report dialog".
    const hidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
    const abuseReportOpen = BrowserTestUtils.waitForCondition(
      () => AbuseReporter.getOpenDialog(),
      "wait for the abuse report dialog to have been opened"
    );
    contextMenu.activateItem(reportButton);
    const [reportDialogWindow] = await Promise.all([abuseReportOpen, hidden]);

    const reportDialogParams = reportDialogWindow.arguments[0].wrappedJSObject;
    is(
      reportDialogParams.report.addon.id,
      extension.id,
      "abuse report dialog has the expected addon id"
    );
    is(
      reportDialogParams.report.reportEntryPoint,
      "unified_context_menu",
      "abuse report dialog has the expected reportEntryPoint"
    );

    let promiseClosedWindow = waitClosedWindow();
    reportDialogWindow.close();
    // Wait for the report dialog window to be completely closed
    // (to prevent an intermittent failure due to a race between
    // the dialog window being closed and the test tasks that follows
    // opening the unified extensions button panel to not lose the
    // focus and be suddently closed before the task has done with
    // its assertions, see Bug 1782304).
    await promiseClosedWindow;
  });

  await extension.unload();
});

add_task(async function test_remove_extension() {
  Services.telemetry.clearEvents();

  const [extension] = createExtensions([{ name: "an extension" }]);
  await extension.startup();

  // Open the extension panel, then open the context menu for the extension.
  await openExtensionsPanel(win);
  const contextMenu = await openUnifiedExtensionsContextMenu(win, extension.id);

  const removeButton = contextMenu.querySelector(
    ".unified-extensions-context-menu-remove-extension"
  );
  ok(removeButton, "expected remove button");

  // Set up a mock prompt service that returns 0 to indicate that the user
  // pressed the OK button.
  const { prompt } = Services;
  const promptService = {
    QueryInterface: ChromeUtils.generateQI(["nsIPromptService"]),
    confirmEx() {
      return 0;
    },
  };
  Services.prompt = promptService;
  registerCleanupFunction(() => {
    Services.prompt = prompt;
  });

  // Click the "remove extension" context menu item, and wait until the menu is
  // closed and the extension is uninstalled.
  const uninstalled = promiseExtensionUninstalled(extension.id);
  const hidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  contextMenu.activateItem(removeButton);
  await Promise.all([uninstalled, hidden]);

  await extension.unload();
  // Restore prompt service.
  Services.prompt = prompt;

  TelemetryTestUtils.assertEvents(
    [
      {
        object: "unifiedExtensions",
        value: "accepted",
        extra: { addonId: extension.id, action: "uninstall" },
      },
    ],
    TELEMETRY_EVENTS_FILTERS
  );
});

add_task(async function test_remove_extension_cancelled() {
  Services.telemetry.clearEvents();

  const [extension] = createExtensions([{ name: "an extension" }]);
  await extension.startup();

  // Open the extension panel, then open the context menu for the extension.
  await openExtensionsPanel(win);
  const contextMenu = await openUnifiedExtensionsContextMenu(win, extension.id);

  const removeButton = contextMenu.querySelector(
    ".unified-extensions-context-menu-remove-extension"
  );
  ok(removeButton, "expected remove button");

  // Set up a mock prompt service that returns 1 to indicate that the user
  // refused to uninstall the extension.
  const { prompt } = Services;
  const promptService = {
    QueryInterface: ChromeUtils.generateQI(["nsIPromptService"]),
    confirmEx() {
      return 1;
    },
  };
  Services.prompt = promptService;
  registerCleanupFunction(() => {
    Services.prompt = prompt;
  });

  // Click the "remove extension" context menu item, and wait until the menu is
  // closed.
  const hidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  contextMenu.activateItem(removeButton);
  await hidden;

  // Re-open the panel to make sure the extension is still there.
  await openExtensionsPanel(win);
  const item = getUnifiedExtensionsItem(win, extension.id);
  is(
    item.querySelector(".unified-extensions-item-name").textContent,
    "an extension",
    "expected extension to still be listed"
  );
  await closeExtensionsPanel(win);

  await extension.unload();
  // Restore prompt service.
  Services.prompt = prompt;

  TelemetryTestUtils.assertEvents(
    [
      {
        object: "unifiedExtensions",
        value: "cancelled",
        extra: { addonId: extension.id, action: "uninstall" },
      },
    ],
    TELEMETRY_EVENTS_FILTERS
  );
});

add_task(async function test_open_context_menu_on_click() {
  const [extension] = createExtensions([{ name: "an extension" }]);
  await extension.startup();

  // Open the extension panel.
  await openExtensionsPanel(win);

  const button = getUnifiedExtensionsItem(win, extension.id).querySelector(
    ".unified-extensions-item-open-menu"
  );
  ok(button, "expected 'open menu' button");

  const contextMenu = win.document.getElementById(
    "unified-extensions-context-menu"
  );
  ok(contextMenu, "expected menu");

  // Open the context menu with a "right-click".
  const shown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(button, { type: "contextmenu" }, win);
  await shown;

  await closeChromeContextMenu(contextMenu.id, null, win);
  await closeExtensionsPanel(win);

  await extension.unload();
});

add_task(async function test_open_context_menu_with_keyboard() {
  const [extension] = createExtensions([{ name: "an extension" }]);
  await extension.startup();

  // Open the extension panel.
  await openExtensionsPanel(win);

  const button = getUnifiedExtensionsItem(win, extension.id).querySelector(
    ".unified-extensions-item-open-menu"
  );
  ok(button, "expected 'open menu' button");
  // Make this button focusable because those (toolbar) buttons are only made
  // focusable when a user is navigating with the keyboard, which isn't exactly
  // what we are doing in this test.
  button.setAttribute("tabindex", "-1");

  const contextMenu = win.document.getElementById(
    "unified-extensions-context-menu"
  );
  ok(contextMenu, "expected menu");

  // Open the context menu by focusing the button and pressing the SPACE key.
  let shown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  button.focus();
  is(button, win.document.activeElement, "expected button to be focused");
  EventUtils.synthesizeKey(" ", {}, win);
  await shown;

  await closeChromeContextMenu(contextMenu.id, null, win);

  // Open the context menu by focusing the button and pressing the ENTER key.
  shown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  button.focus();
  is(button, win.document.activeElement, "expected button to be focused");
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  await shown;

  await closeChromeContextMenu(contextMenu.id, null, win);
  await closeExtensionsPanel(win);

  await extension.unload();
});

add_task(async function test_context_menu_without_browserActionFor_global() {
  const { ExtensionParent } = ChromeUtils.import(
    "resource://gre/modules/ExtensionParent.jsm"
  );
  const { browserActionFor } = ExtensionParent.apiManager.global;
  const cleanup = () => {
    ExtensionParent.apiManager.global.browserActionFor = browserActionFor;
  };
  registerCleanupFunction(cleanup);
  // This is needed to simulate the case where the browserAction API hasn't
  // been loaded yet (since it is lazy-loaded). That could happen when only
  // extensions without browser actions are installed. In which case, the
  // `global.browserActionFor()` function would not be defined yet.
  delete ExtensionParent.apiManager.global.browserActionFor;

  const [extension] = createExtensions([{ name: "an extension" }]);
  await extension.startup();

  // Open the extension panel and then the context menu for the extension that
  // has been loaded above. We expect the context menu to be displayed and no
  // error caused by the lack of `global.browserActionFor()`.
  await openExtensionsPanel(win);
  // This promise rejects with an error if the implementation does not handle
  // the case where `global.browserActionFor()` is undefined.
  const contextMenu = await openUnifiedExtensionsContextMenu(win, extension.id);
  is(contextMenu.childElementCount, 3, "expected 3 menu items");

  await closeChromeContextMenu(contextMenu.id, null, win);
  await closeExtensionsPanel(win);

  await extension.unload();

  cleanup();
});

add_task(async function test_browser_action_context_menu() {
  const extWithMenuBrowserAction = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {},
      permissions: ["contextMenus"],
    },
    background() {
      browser.contextMenus.create(
        {
          id: "some-menu-id",
          title: "Click me!",
          contexts: ["all"],
        },
        () => browser.test.sendMessage("ready")
      );
    },
    useAddonManager: "temporary",
  });
  const extWithSubMenuBrowserAction = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {},
      permissions: ["contextMenus"],
    },
    background() {
      browser.contextMenus.create({
        id: "some-menu-id",
        title: "Open sub-menu",
        contexts: ["all"],
      });
      browser.contextMenus.create(
        {
          id: "some-sub-menu-id",
          parentId: "some-menu-id",
          title: "Click me!",
          contexts: ["all"],
        },
        () => browser.test.sendMessage("ready")
      );
    },
    useAddonManager: "temporary",
  });
  const extWithMenuPageAction = ExtensionTestUtils.loadExtension({
    manifest: {
      page_action: {},
      permissions: ["contextMenus"],
    },
    background() {
      browser.contextMenus.create(
        {
          id: "some-menu-id",
          title: "Click me!",
          contexts: ["all"],
        },
        () => browser.test.sendMessage("ready")
      );
    },
    useAddonManager: "temporary",
  });
  const extWithoutMenu1 = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "extension without any menu",
    },
    useAddonManager: "temporary",
  });
  const extWithoutMenu2 = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {},
      name: "extension with a browser action but no menu",
    },
    useAddonManager: "temporary",
  });

  const extensions = [
    extWithMenuBrowserAction,
    extWithMenuPageAction,
    extWithSubMenuBrowserAction,
    extWithoutMenu1,
    extWithoutMenu2,
  ];

  await Promise.all(extensions.map(extension => extension.startup()));

  await extWithMenuBrowserAction.awaitMessage("ready");
  await extWithMenuPageAction.awaitMessage("ready");
  await extWithSubMenuBrowserAction.awaitMessage("ready");

  await openExtensionsPanel(win);

  info("extension with browser action and a menu");
  // The context menu for the extension that declares a browser action menu
  // should have the memu item created by the extension, a menu separator and
  // the 3 default menu items.
  let contextMenu = await openUnifiedExtensionsContextMenu(
    win,
    extWithMenuBrowserAction.id
  );
  is(contextMenu.childElementCount, 5, "expected 5 menu items");
  const [item, separator] = contextMenu.children;
  is(
    item.getAttribute("label"),
    "Click me!",
    "expected menu item as first child"
  );
  is(
    separator.tagName,
    "menuseparator",
    "expected separator after last menu item created by the extension"
  );
  await closeChromeContextMenu(contextMenu.id, null, win);

  info("extension with page action and a menu");
  // This extension declares a page action so its menu shouldn't be added to
  // the unified extensions context menu.
  contextMenu = await openUnifiedExtensionsContextMenu(
    win,
    extWithMenuPageAction.id
  );
  is(contextMenu.childElementCount, 3, "expected 3 menu items");
  await closeChromeContextMenu(contextMenu.id, null, win);

  info("extension with no browser action and no menu");
  // There is no context menu created by this extension, so there should only
  // be 3 menu items corresponding to the default manage/remove/report items.
  contextMenu = await openUnifiedExtensionsContextMenu(win, extWithoutMenu1.id);
  is(contextMenu.childElementCount, 3, "expected 3 menu items");
  await closeChromeContextMenu(contextMenu.id, null, win);

  info("extension with browser action and no menu");
  // There is no context menu created by this extension, so there should only
  // be 3 menu items corresponding to the default manage/remove/report items.
  contextMenu = await openUnifiedExtensionsContextMenu(win, extWithoutMenu2.id);
  is(contextMenu.childElementCount, 3, "expected 3 menu items");
  await closeChromeContextMenu(contextMenu.id, null, win);

  info("extension with browser action and menu + sub-menu");
  // Open a context menu that has a sub-menu and verify that we can get to the
  // sub-menu item.
  contextMenu = await openUnifiedExtensionsContextMenu(
    win,
    extWithSubMenuBrowserAction.id
  );
  is(contextMenu.childElementCount, 5, "expected 5 menu items");
  const popup = await openSubmenu(contextMenu.children[0]);
  is(popup.children.length, 1, "expected 1 submenu item");
  is(
    popup.children[0].getAttribute("label"),
    "Click me!",
    "expected menu item"
  );
  // The number of items in the (main) context menu should remain the same.
  is(contextMenu.childElementCount, 5, "expected 5 menu items");
  await closeChromeContextMenu(contextMenu.id, null, win);

  await closeExtensionsPanel(win);

  await Promise.all(extensions.map(extension => extension.unload()));
});
