/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AbuseReporter",
  "resource://gre/modules/AbuseReporter.jsm"
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "ABUSE_REPORT_ENABLED",
  "extensions.abuseReport.enabled",
  false
);

let extData = {
  manifest: {
    permissions: ["contextMenus"],
    browser_action: {
      default_popup: "popup.html",
    },
  },
  useAddonManager: "temporary",

  files: {
    "popup.html": `
      <!DOCTYPE html>
      <html>
      <head><meta charset="utf-8"/>
      </head>
      <body>
      <span id="text">A Test Popup</span>
      <img id="testimg" src="data:image/svg+xml,<svg></svg>" height="10" width="10">
      </body></html>
    `,
  },

  background: function() {
    browser.contextMenus.create({
      id: "clickme-page",
      title: "Click me!",
      contexts: ["all"],
    });
  },
};

let contextMenuItems = {
  "context-navigation": "hidden",
  "context-sep-navigation": "hidden",
  "context-viewsource": "",
  "inspect-separator": "hidden",
  "context-inspect": "hidden",
  "context-inspect-a11y": "hidden",
  "context-bookmarkpage": "hidden",
};

const type = "extension";

function assertTelemetryMatches(events) {
  events = events.map(([method, object, value, extra]) => {
    return { method, object, value, extra };
  });
  TelemetryTestUtils.assertEvents(events, {
    category: "addonsManager",
    method: /^(action|link|view)$/,
  });
}

add_task(async function test_setup() {
  // Clear any previosuly collected telemetry event.
  Services.telemetry.clearEvents();
  if (CustomizableUI.protonToolbarEnabled) {
    CustomizableUI.addWidgetToArea("home-button", "nav-bar");
    registerCleanupFunction(() =>
      CustomizableUI.removeWidgetFromArea("home-button")
    );
  }
});

add_task(async function browseraction_popup_contextmenu() {
  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();

  await clickBrowserAction(extension, window);

  let contentAreaContextMenu = await openContextMenuInPopup(extension);
  let item = contentAreaContextMenu.getElementsByAttribute(
    "label",
    "Click me!"
  );
  is(item.length, 1, "contextMenu item for page was found");
  await closeContextMenu(contentAreaContextMenu);

  await extension.unload();
});

add_task(async function browseraction_popup_contextmenu_hidden_items() {
  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();

  await clickBrowserAction(extension);

  let contentAreaContextMenu = await openContextMenuInPopup(extension, "#text");

  let item, state;
  for (const itemID in contextMenuItems) {
    item = contentAreaContextMenu.querySelector(`#${itemID}`);
    state = contextMenuItems[itemID];

    if (state !== "") {
      ok(item[state], `${itemID} is ${state}`);

      if (state !== "hidden") {
        ok(!item.hidden, `Disabled ${itemID} is not hidden`);
      }
    } else {
      ok(!item.hidden, `${itemID} is not hidden`);
      ok(!item.disabled, `${itemID} is not disabled`);
    }
  }

  await closeContextMenu(contentAreaContextMenu);

  await extension.unload();
});

add_task(async function browseraction_popup_image_contextmenu() {
  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();

  await clickBrowserAction(extension);

  let contentAreaContextMenu = await openContextMenuInPopup(
    extension,
    "#testimg"
  );

  let item = contentAreaContextMenu.querySelector("#context-copyimage");
  ok(!item.hidden);
  ok(!item.disabled);

  await closeContextMenu(contentAreaContextMenu);

  await extension.unload();
});

function openContextMenu(menuId, targetId, win = window) {
  return openChromeContextMenu(menuId, "#" + CSS.escape(targetId), win);
}

function waitForElementShown(element) {
  let win = element.ownerGlobal;
  let dwu = win.windowUtils;
  return BrowserTestUtils.waitForCondition(() => {
    info("Waiting for overflow button to have non-0 size");
    let bounds = dwu.getBoundsWithoutFlushing(element);
    return bounds.width > 0 && bounds.height > 0;
  });
}

add_task(async function browseraction_contextmenu_manage_extension() {
  // Do the customize mode shuffle in a separate window, because it interferes
  // with other tests.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let id = "addon_id@example.com";
  let buttonId = `${makeWidgetId(id)}-browser-action`;
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: { id },
      },
      browser_action: {},
      options_ui: {
        page: "options.html",
      },
    },
    useAddonManager: "temporary",
    files: {
      "options.html": `<script src="options.js"></script>`,
      "options.js": `browser.test.sendMessage("options-loaded");`,
    },
  });

  function checkVisibility(menu, visible) {
    let removeExtension = menu.querySelector(
      ".customize-context-removeExtension"
    );
    let manageExtension = menu.querySelector(
      ".customize-context-manageExtension"
    );
    let reportExtension = menu.querySelector(
      ".customize-context-reportExtension"
    );
    let separator = reportExtension.nextElementSibling;

    info(`Check visibility: ${visible}`);
    let expected = visible ? "visible" : "hidden";
    is(
      removeExtension.hidden,
      !visible,
      `Remove Extension should be ${expected}`
    );
    is(
      manageExtension.hidden,
      !visible,
      `Manage Extension should be ${expected}`
    );
    is(
      reportExtension.hidden,
      !ABUSE_REPORT_ENABLED || !visible,
      `Report Extension should be ${expected}`
    );
    is(
      separator.hidden,
      !visible,
      `Separator after Manage Extension should be ${expected}`
    );
  }

  async function testContextMenu(menuId, customizing) {
    info(`Open browserAction context menu in ${menuId}`);
    let menu = await openContextMenu(menuId, buttonId, win);
    await checkVisibility(menu, true);

    info(`Choosing 'Manage Extension' in ${menuId} should load options`);
    let addonManagerPromise = BrowserTestUtils.waitForNewTab(
      win.gBrowser,
      "about:addons",
      true
    );
    let manageExtension = menu.querySelector(
      ".customize-context-manageExtension"
    );
    await closeChromeContextMenu(menuId, manageExtension, win);
    let managerWindow = (await addonManagerPromise).linkedBrowser.contentWindow;

    // Check the UI to make sure that the correct view is loaded.
    is(
      managerWindow.gViewController.currentViewId,
      `addons://detail/${encodeURIComponent(id)}`,
      "Expected extension details view in about:addons"
    );
    // In HTML about:addons, the default view does not show the inline
    // options browser, so we should not receive an "options-loaded" event.
    // (if we do, the test will fail due to the unexpected message).

    info(
      `Remove the opened tab, and await customize mode to be restored if necessary`
    );
    let tab = win.gBrowser.selectedTab;
    is(tab.linkedBrowser.currentURI.spec, "about:addons");
    if (customizing) {
      let customizationReady = BrowserTestUtils.waitForEvent(
        win.gNavToolbox,
        "customizationready"
      );
      win.gBrowser.removeTab(tab);
      await customizationReady;
    } else {
      win.gBrowser.removeTab(tab);
    }

    return menu;
  }

  async function main(customizing) {
    if (customizing) {
      info("Enter customize mode");
      let customizationReady = BrowserTestUtils.waitForEvent(
        win.gNavToolbox,
        "customizationready"
      );
      win.gCustomizeMode.enter();
      await customizationReady;
    }

    info("Test toolbar context menu in browserAction");
    let toolbarCtxMenu = await testContextMenu(
      "toolbar-context-menu",
      customizing
    );

    info("Check toolbar context menu in another button");
    let otherButtonId = "home-button";
    await openContextMenu(toolbarCtxMenu.id, otherButtonId, win);
    checkVisibility(toolbarCtxMenu, false);
    toolbarCtxMenu.hidePopup();

    info("Check toolbar context menu without triggerNode");
    toolbarCtxMenu.openPopup();
    checkVisibility(toolbarCtxMenu, false);
    toolbarCtxMenu.hidePopup();

    info("Pin the browserAction and another button to the overflow menu");
    CustomizableUI.addWidgetToArea(
      buttonId,
      CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
    );
    CustomizableUI.addWidgetToArea(
      otherButtonId,
      CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
    );

    info("Wait until the overflow menu is ready");
    let overflowButton = win.document.getElementById("nav-bar-overflow-button");
    let icon = overflowButton.icon;
    await waitForElementShown(icon);

    if (!customizing) {
      info("Open overflow menu");
      let menu = win.document.getElementById("widget-overflow");
      let shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
      overflowButton.click();
      await shown;
    }

    info("Check overflow menu context menu in another button");
    let overflowMenuCtxMenu = await openContextMenu(
      "customizationPanelItemContextMenu",
      otherButtonId,
      win
    );
    checkVisibility(overflowMenuCtxMenu, false);
    overflowMenuCtxMenu.hidePopup();

    info("Test overflow menu context menu in browserAction");
    await testContextMenu(overflowMenuCtxMenu.id, customizing);

    info("Restore initial state");
    CustomizableUI.addWidgetToArea(buttonId, CustomizableUI.AREA_NAVBAR);
    CustomizableUI.addWidgetToArea(otherButtonId, CustomizableUI.AREA_NAVBAR);

    if (customizing) {
      info("Exit customize mode");
      let afterCustomization = BrowserTestUtils.waitForEvent(
        win.gNavToolbox,
        "aftercustomization"
      );
      win.gCustomizeMode.exit();
      await afterCustomization;
    }
  }

  await extension.startup();

  info(
    "Add a dummy tab to prevent about:addons from being loaded in the initial about:blank tab"
  );
  let dummyTab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    "http://example.com",
    true,
    true
  );

  info("Run tests in normal mode");
  await main(false);
  assertTelemetryMatches([
    ["action", "browserAction", null, { action: "manage", addonId: id }],
    ["view", "aboutAddons", "detail", { addonId: id, type }],
    ["action", "browserAction", null, { action: "manage", addonId: id }],
    ["view", "aboutAddons", "detail", { addonId: id, type }],
  ]);

  info("Run tests in customize mode");
  await main(true);
  assertTelemetryMatches([
    ["action", "browserAction", null, { action: "manage", addonId: id }],
    ["view", "aboutAddons", "detail", { addonId: id, type }],
    ["action", "browserAction", null, { action: "manage", addonId: id }],
    ["view", "aboutAddons", "detail", { addonId: id, type }],
  ]);

  info("Close the dummy tab and finish");
  win.gBrowser.removeTab(dummyTab);
  await extension.unload();

  await BrowserTestUtils.closeWindow(win);
});

async function runTestContextMenu({
  buttonId,
  customizing,
  testContextMenu,
  win,
}) {
  if (customizing) {
    info("Enter customize mode");
    let customizationReady = BrowserTestUtils.waitForEvent(
      win.gNavToolbox,
      "customizationready"
    );
    win.gCustomizeMode.enter();
    await customizationReady;
  }

  info("Test toolbar context menu in browserAction");
  await testContextMenu("toolbar-context-menu", customizing);

  info("Pin the browserAction and another button to the overflow menu");
  CustomizableUI.addWidgetToArea(
    buttonId,
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );

  info("Wait until the overflow menu is ready");
  let overflowButton = win.document.getElementById("nav-bar-overflow-button");
  let icon = overflowButton.icon;
  await waitForElementShown(icon);

  if (!customizing) {
    info("Open overflow menu");
    let menu = win.document.getElementById("widget-overflow");
    let shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
    overflowButton.click();
    await shown;
  }

  info("Test overflow menu context menu in browserAction");
  await testContextMenu("customizationPanelItemContextMenu", customizing);

  info("Restore initial state");
  CustomizableUI.addWidgetToArea(buttonId, CustomizableUI.AREA_NAVBAR);

  if (customizing) {
    info("Exit customize mode");
    let afterCustomization = BrowserTestUtils.waitForEvent(
      win.gNavToolbox,
      "aftercustomization"
    );
    win.gCustomizeMode.exit();
    await afterCustomization;
  }
}

add_task(async function browseraction_contextmenu_remove_extension() {
  // Do the customize mode shuffle in a separate window, because it interferes
  // with other tests.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let id = "addon_id@example.com";
  let name = "Awesome Add-on";
  let buttonId = `${makeWidgetId(id)}-browser-action`;
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name,
      applications: {
        gecko: { id },
      },
      browser_action: {},
    },
    useAddonManager: "temporary",
  });
  let brand = Services.strings
    .createBundle("chrome://branding/locale/brand.properties")
    .GetStringFromName("brandShorterName");
  let { prompt } = Services;
  let promptService = {
    _response: 1,
    QueryInterface: ChromeUtils.generateQI(["nsIPromptService"]),
    confirmEx: function(...args) {
      promptService._confirmExArgs = args;
      return promptService._response;
    },
  };
  Services.prompt = promptService;
  registerCleanupFunction(() => {
    Services.prompt = prompt;
  });

  async function testContextMenu(menuId, customizing) {
    info(`Open browserAction context menu in ${menuId}`);
    let menu = await openContextMenu(menuId, buttonId, win);

    info(`Choosing 'Remove Extension' in ${menuId} should show confirm dialog`);
    let removeExtension = menu.querySelector(
      ".customize-context-removeExtension"
    );
    await closeChromeContextMenu(menuId, removeExtension, win);
    is(promptService._confirmExArgs[1], `Remove ${name}`);
    is(promptService._confirmExArgs[2], `Remove ${name} from ${brand}?`);
    is(promptService._confirmExArgs[4], "Remove");
    return menu;
  }

  await extension.startup();

  info("Run tests in normal mode");
  await runTestContextMenu({
    buttonId,
    customizing: false,
    testContextMenu,
    win,
  });

  assertTelemetryMatches([
    [
      "action",
      "browserAction",
      "cancelled",
      { action: "uninstall", addonId: id },
    ],
    [
      "action",
      "browserAction",
      "cancelled",
      { action: "uninstall", addonId: id },
    ],
  ]);

  info("Run tests in customize mode");
  await runTestContextMenu({
    buttonId,
    customizing: true,
    testContextMenu,
    win,
  });

  assertTelemetryMatches([
    [
      "action",
      "browserAction",
      "cancelled",
      { action: "uninstall", addonId: id },
    ],
    [
      "action",
      "browserAction",
      "cancelled",
      { action: "uninstall", addonId: id },
    ],
  ]);

  let addon = await AddonManager.getAddonByID(id);
  ok(addon, "Addon is still installed");

  promptService._response = 0;
  let uninstalled = new Promise(resolve => {
    AddonManager.addAddonListener({
      onUninstalled(addon) {
        is(addon.id, id, "The expected add-on has been uninstalled");
        AddonManager.removeAddonListener(this);
        resolve();
      },
    });
  });
  await testContextMenu("toolbar-context-menu", false);
  await uninstalled;

  assertTelemetryMatches([
    [
      "action",
      "browserAction",
      "accepted",
      { action: "uninstall", addonId: id },
    ],
  ]);

  addon = await AddonManager.getAddonByID(id);
  ok(!addon, "Addon has been uninstalled");

  await extension.unload();

  await BrowserTestUtils.closeWindow(win);
});

// This test case verify reporting an extension from the browserAction
// context menu (when the browserAction is in the toolbox and in the
// overwflow menu, and repeat the test with and without the customize
// mode enabled).
add_task(async function browseraction_contextmenu_report_extension() {
  SpecialPowers.pushPrefEnv({
    set: [["extensions.abuseReport.enabled", true]],
  });
  let win;
  let id = "addon_id@example.com";
  let name = "Bad Add-on";
  let buttonId = `${makeWidgetId(id)}-browser-action`;
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name,
      author: "Bad author",
      applications: {
        gecko: { id },
      },
      browser_action: {},
    },
    useAddonManager: "temporary",
  });

  async function testReportDialog() {
    const reportDialogWindow = await BrowserTestUtils.waitForCondition(
      () => AbuseReporter.getOpenDialog(),
      "Wait for the abuse report dialog to have been opened"
    );

    const reportDialogParams = reportDialogWindow.arguments[0].wrappedJSObject;
    is(
      reportDialogParams.report.addon.id,
      id,
      "Abuse report dialog has the expected addon id"
    );
    is(
      reportDialogParams.report.reportEntryPoint,
      "toolbar_context_menu",
      "Abuse report dialog has the expected reportEntryPoint"
    );

    info("Wait the report dialog to complete rendering");
    await reportDialogParams.promiseReportPanel;
    info("Close the report dialog");
    reportDialogWindow.close();
    is(
      await reportDialogParams.promiseReport,
      undefined,
      "Report resolved as user cancelled when the window is closed"
    );
  }

  async function testContextMenu(menuId, customizing) {
    info(`Open browserAction context menu in ${menuId}`);
    let menu = await openContextMenu(menuId, buttonId, win);

    info(`Choosing 'Report Extension' in ${menuId} should show confirm dialog`);
    let reportExtension = menu.querySelector(
      ".customize-context-reportExtension"
    );
    ok(!reportExtension.hidden, "Report extension should be visibile");

    // When running in customizing mode "about:addons" will load in a new tab,
    // otherwise it will replace the existing blank tab.
    const onceAboutAddonsTab = customizing
      ? BrowserTestUtils.waitForNewTab(win.gBrowser, "about:addons")
      : BrowserTestUtils.waitForCondition(() => {
          return win.gBrowser.currentURI.spec === "about:addons";
        }, "Wait an about:addons tab to be opened");

    await closeChromeContextMenu(menuId, reportExtension, win);
    await onceAboutAddonsTab;

    const browser = win.gBrowser.selectedBrowser;
    is(
      browser.currentURI.spec,
      "about:addons",
      "Got about:addons tab selected"
    );

    // Do not wait for the about:addons tab to be loaded if its
    // document is already readyState==complete.
    // This prevents intermittent timeout failures while running
    // this test in optimized builds.
    if (browser.contentDocument?.readyState != "complete") {
      await BrowserTestUtils.browserLoaded(browser);
    }
    await testReportDialog();

    // Close the new about:addons tab when running in customize mode,
    // or cancel the abuse report if the about:addons page has been
    // loaded in the existing blank tab.
    if (customizing) {
      info("Closing the about:addons tab");
      let customizationReady = BrowserTestUtils.waitForEvent(
        win.gNavToolbox,
        "customizationready"
      );
      win.gBrowser.removeTab(win.gBrowser.selectedTab);
      await customizationReady;
    } else {
      info("Navigate the about:addons tab to about:blank");
      BrowserTestUtils.loadURI(browser, "about:blank");
      await BrowserTestUtils.browserLoaded(browser);
    }

    return menu;
  }

  await extension.startup();

  win = await BrowserTestUtils.openNewBrowserWindow();

  info("Run tests in normal mode");
  await runTestContextMenu({
    buttonId,
    customizing: false,
    testContextMenu,
    win,
  });

  info("Run tests in customize mode");
  await runTestContextMenu({
    buttonId,
    customizing: true,
    testContextMenu,
    win,
  });

  await BrowserTestUtils.closeWindow(win);
  await extension.unload();
});
