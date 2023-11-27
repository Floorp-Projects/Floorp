/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AbuseReporter: "resource://gre/modules/AbuseReporter.sys.mjs",
});

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
      default_area: "navbar",
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

  background: function () {
    browser.contextMenus.create({
      id: "clickme-page",
      title: "Click me!",
      contexts: ["all"],
    });
  },
};

let contextMenuItems = {
  "context-sep-navigation": "hidden",
  "context-viewsource": "",
  "inspect-separator": "hidden",
  "context-inspect": "hidden",
  "context-inspect-a11y": "hidden",
  "context-bookmarkpage": "hidden",
};
if (AppConstants.platform == "macosx") {
  contextMenuItems["context-back"] = "hidden";
  contextMenuItems["context-forward"] = "hidden";
  contextMenuItems["context-reload"] = "hidden";
  contextMenuItems["context-stop"] = "hidden";
} else {
  contextMenuItems["context-navigation"] = "hidden";
}

const TOOLBAR_CONTEXT_MENU = "toolbar-context-menu";
const UNIFIED_CONTEXT_MENU = "unified-extensions-context-menu";

loadTestSubscript("head_unified_extensions.js");

add_setup(async function test_setup() {
  CustomizableUI.addWidgetToArea("home-button", "nav-bar");
  registerCleanupFunction(() =>
    CustomizableUI.removeWidgetFromArea("home-button")
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "extensions.abuseReport.amoFormURL",
        "https://example.org/%LOCALE%/%APP%/feedback/addon/%addonID%/",
      ],
    ],
  });
});

async function browseraction_popup_contextmenu_helper() {
  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();

  await clickBrowserAction(extension);

  let contentAreaContextMenu = await openContextMenuInPopup(extension);
  let item = contentAreaContextMenu.getElementsByAttribute(
    "label",
    "Click me!"
  );
  is(item.length, 1, "contextMenu item for page was found");
  await closeContextMenu(contentAreaContextMenu);

  await closeBrowserAction(extension);

  await extension.unload();
}

async function browseraction_popup_contextmenu_hidden_items_helper() {
  let extension = ExtensionTestUtils.loadExtension(extData);
  await extension.startup();

  await clickBrowserAction(extension);

  let contentAreaContextMenu = await openContextMenuInPopup(extension, "#text");

  let item, state;
  for (const itemID in contextMenuItems) {
    info(`Checking ${itemID}`);
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

  await closeBrowserAction(extension);

  await extension.unload();
}

async function browseraction_popup_image_contextmenu_helper() {
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

  await closeBrowserAction(extension);

  await extension.unload();
}

function openContextMenu(menuId, targetId) {
  info(`Open context menu ${menuId} at ${targetId}`);
  return openChromeContextMenu(menuId, "#" + CSS.escape(targetId));
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

async function browseraction_contextmenu_manage_extension_helper() {
  let id = "addon_id@example.com";
  let buttonId = `${makeWidgetId(id)}-BAP`;
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: { id },
      },
      browser_action: {
        default_area: "navbar",
      },
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
    info(`Open browserAction context menu in ${menuId} on ${buttonId}`);
    let menu = await openContextMenu(menuId, buttonId);
    await checkVisibility(menu, true);

    info(`Choosing 'Manage Extension' in ${menuId} should load options`);
    let addonManagerPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      "about:addons",
      true
    );
    let manageExtension = menu.querySelector(
      ".customize-context-manageExtension"
    );
    await closeChromeContextMenu(menuId, manageExtension);
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
    let tab = gBrowser.selectedTab;
    is(tab.linkedBrowser.currentURI.spec, "about:addons");
    if (customizing) {
      let customizationReady = BrowserTestUtils.waitForEvent(
        gNavToolbox,
        "customizationready"
      );
      gBrowser.removeTab(tab);
      await customizationReady;
    } else {
      gBrowser.removeTab(tab);
    }

    return menu;
  }

  async function main(customizing) {
    if (customizing) {
      info("Enter customize mode");
      let customizationReady = BrowserTestUtils.waitForEvent(
        gNavToolbox,
        "customizationready"
      );
      gCustomizeMode.enter();
      await customizationReady;
    }

    info("Test toolbar context menu in browserAction");
    let toolbarCtxMenu = await testContextMenu(
      TOOLBAR_CONTEXT_MENU,
      customizing
    );

    info("Check toolbar context menu in another button");
    let otherButtonId = "home-button";
    await openContextMenu(TOOLBAR_CONTEXT_MENU, otherButtonId);
    checkVisibility(toolbarCtxMenu, false);
    toolbarCtxMenu.hidePopup();

    info("Check toolbar context menu without triggerNode");
    toolbarCtxMenu.openPopup();
    checkVisibility(toolbarCtxMenu, false);
    toolbarCtxMenu.hidePopup();

    CustomizableUI.addWidgetToArea(
      otherButtonId,
      CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
    );

    info("Wait until the overflow menu is ready");
    let overflowButton = document.getElementById("nav-bar-overflow-button");
    let icon = overflowButton.icon;
    await waitForElementShown(icon);

    if (!customizing) {
      info("Open overflow menu");
      let menu = document.getElementById("widget-overflow");
      let shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
      overflowButton.click();
      await shown;
    }

    info("Check overflow menu context menu in another button");
    let overflowMenuCtxMenu = await openContextMenu(
      "customizationPanelItemContextMenu",
      otherButtonId
    );
    checkVisibility(overflowMenuCtxMenu, false);
    overflowMenuCtxMenu.hidePopup();

    info("Put other button action back in nav-bar");
    CustomizableUI.addWidgetToArea(otherButtonId, CustomizableUI.AREA_NAVBAR);

    if (customizing) {
      info("Exit customize mode");
      let afterCustomization = BrowserTestUtils.waitForEvent(
        gNavToolbox,
        "aftercustomization"
      );
      gCustomizeMode.exit();
      await afterCustomization;
    }
  }

  await extension.startup();

  info(
    "Add a dummy tab to prevent about:addons from being loaded in the initial about:blank tab"
  );
  let dummyTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com",
    true,
    true
  );

  info("Run tests in normal mode");
  await main(false);

  info("Run tests in customize mode");
  await main(true);

  info("Close the dummy tab and finish");
  gBrowser.removeTab(dummyTab);
  await extension.unload();
}

async function runTestContextMenu({ id, customizing, testContextMenu }) {
  let widgetId = makeWidgetId(id);
  let nodeId = `${widgetId}-browser-action`;
  if (customizing) {
    info("Enter customize mode");
    let customizationReady = BrowserTestUtils.waitForEvent(
      gNavToolbox,
      "customizationready"
    );
    gCustomizeMode.enter();
    await customizationReady;
  }

  info("Test toolbar context menu in browserAction");
  await testContextMenu(TOOLBAR_CONTEXT_MENU, customizing);

  info("Pin the browserAction to the addons panel");
  CustomizableUI.addWidgetToArea(nodeId, CustomizableUI.AREA_ADDONS);

  if (!customizing) {
    info("Open addons panel");
    gUnifiedExtensions.togglePanel();
    await BrowserTestUtils.waitForEvent(gUnifiedExtensions.panel, "popupshown");
    info("Test browserAction in addons panel");
    await testContextMenu(UNIFIED_CONTEXT_MENU, customizing);
  } else {
    todo(
      false,
      "The browserAction cannot be accessed from customize " +
        "mode when in the addons panel."
    );
  }

  info("Restore initial state");
  CustomizableUI.addWidgetToArea(nodeId, CustomizableUI.AREA_NAVBAR);

  if (customizing) {
    info("Exit customize mode");
    let afterCustomization = BrowserTestUtils.waitForEvent(
      gNavToolbox,
      "aftercustomization"
    );
    gCustomizeMode.exit();
    await afterCustomization;
  }
}

async function browseraction_contextmenu_remove_extension_helper() {
  let id = "addon_id@example.com";
  let name = "Awesome Add-on";
  let buttonId = `${makeWidgetId(id)}-BAP`;
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name,
      browser_specific_settings: {
        gecko: { id },
      },
      browser_action: {
        default_area: "navbar",
      },
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
    confirmEx: function (...args) {
      promptService._resolveArgs(args);
      return promptService._response;
    },
    confirmArgs() {
      return new Promise(resolve => {
        promptService._resolveArgs = resolve;
      });
    },
  };
  Services.prompt = promptService;
  registerCleanupFunction(() => {
    Services.prompt = prompt;
  });

  async function testContextMenu(menuId, customizing) {
    info(`Open browserAction context menu in ${menuId}`);
    let confirmArgs = promptService.confirmArgs();
    let menu = await openContextMenu(menuId, buttonId);

    info(`Choosing 'Remove Extension' in ${menuId} should show confirm dialog`);
    let removeItemQuery =
      menuId == UNIFIED_CONTEXT_MENU
        ? ".unified-extensions-context-menu-remove-extension"
        : ".customize-context-removeExtension";
    let removeExtension = menu.querySelector(removeItemQuery);
    await closeChromeContextMenu(menuId, removeExtension);
    let args = await confirmArgs;
    is(args[1], `Remove ${name}?`);
    if (!Services.prefs.getBoolPref("prompts.windowPromptSubDialog", false)) {
      is(args[2], `Remove ${name} from ${brand}?`);
    }
    is(args[4], "Remove");
    return menu;
  }

  await extension.startup();

  info("Run tests in normal mode");
  await runTestContextMenu({
    id,
    customizing: false,
    testContextMenu,
  });

  info("Run tests in customize mode");
  await runTestContextMenu({
    id,
    customizing: true,
    testContextMenu,
  });

  // We'll only get one of these because in customize mode, the browserAction
  // is not accessible when in the addons panel.
  todo(
    false,
    "Should record a second removal event when browserAction " +
      "becomes available in customize mode."
  );

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
  await testContextMenu(TOOLBAR_CONTEXT_MENU, false);
  await uninstalled;

  addon = await AddonManager.getAddonByID(id);
  ok(!addon, "Addon has been uninstalled");

  await extension.unload();

  // We've got a cleanup function registered to restore this, but on debug
  // builds, it seems that sometimes the cleanup function won't run soon
  // enough and we'll leak this window because of the fake prompt function
  // staying alive on Services. We work around this by restoring prompt
  // here within the test if we've gotten here without throwing.
  Services.prompt = prompt;
}

// This test case verify reporting an extension from the browserAction
// context menu (when the browserAction is in the toolbox and in the
// overwflow menu, and repeat the test with and without the customize
// mode enabled).
async function browseraction_contextmenu_report_extension_helper() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.abuseReport.enabled", true]],
  });

  let id = "addon_id@example.com";
  let name = "Bad Add-on";
  let buttonId = `${makeWidgetId(id)}-browser-action`;
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name,
      author: "Bad author",
      browser_specific_settings: {
        gecko: { id },
      },
      browser_action: {
        default_area: "navbar",
      },
    },
    useAddonManager: "temporary",
  });

  async function testReportDialog(viaUnifiedContextMenu) {
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
      viaUnifiedContextMenu ? "unified_context_menu" : "toolbar_context_menu",
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
    let menu = await openContextMenu(menuId, buttonId);

    info(`Choosing 'Report Extension' in ${menuId}`);

    let usingUnifiedContextMenu = menuId == UNIFIED_CONTEXT_MENU;
    let reportItemQuery = usingUnifiedContextMenu
      ? ".unified-extensions-context-menu-report-extension"
      : ".customize-context-reportExtension";
    let reportExtension = menu.querySelector(reportItemQuery);

    ok(!reportExtension.hidden, "Report extension should be visibile");

    let aboutAddonsBrowser;

    if (AbuseReporter.amoFormEnabled) {
      const reportURL = Services.urlFormatter
        .formatURLPref("extensions.abuseReport.amoFormURL")
        .replace("%addonID%", id);

      const promiseReportTab = BrowserTestUtils.waitForNewTab(
        gBrowser,
        reportURL,
        /* waitForLoad */ false,
        // Expect it to be the next tab opened
        /* waitForAnyTab */ false
      );
      await closeChromeContextMenu(menuId, reportExtension);
      const reportTab = await promiseReportTab;
      // Remove the report tab and expect the selected tab
      // to become the about:addons tab.
      BrowserTestUtils.removeTab(reportTab);
      is(
        gBrowser.selectedBrowser.currentURI.spec,
        "about:blank",
        "Expect about:addons tab to not have been opened (amoFormEnabled=true)"
      );
    } else {
      // When running in customizing mode "about:addons" will load in a new tab,
      // otherwise it will replace the existing blank tab.
      const onceAboutAddonsTab = customizing
        ? BrowserTestUtils.waitForNewTab(gBrowser, "about:addons")
        : BrowserTestUtils.waitForCondition(() => {
            return gBrowser.currentURI.spec === "about:addons";
          }, "Wait an about:addons tab to be opened");
      await closeChromeContextMenu(menuId, reportExtension);
      await onceAboutAddonsTab;
      const browser = gBrowser.selectedBrowser;
      is(
        browser.currentURI.spec,
        "about:addons",
        "Got about:addons tab selected (amoFormEnabled=false)"
      );
      // Do not wait for the about:addons tab to be loaded if its
      // document is already readyState==complete.
      // This prevents intermittent timeout failures while running
      // this test in optimized builds.
      if (browser.contentDocument?.readyState != "complete") {
        await BrowserTestUtils.browserLoaded(browser);
      }
      await testReportDialog(usingUnifiedContextMenu);
      aboutAddonsBrowser = browser;
    }

    // Close the new about:addons tab when running in customize mode,
    // or cancel the abuse report if the about:addons page has been
    // loaded in the existing blank tab.
    if (customizing) {
      info("Closing the about:addons tab");
      let customizationReady = BrowserTestUtils.waitForEvent(
        gNavToolbox,
        "customizationready"
      );
      gBrowser.removeTab(gBrowser.selectedTab);
      await customizationReady;
    } else if (aboutAddonsBrowser) {
      info("Navigate the about:addons tab to about:blank");
      BrowserTestUtils.startLoadingURIString(aboutAddonsBrowser, "about:blank");
      await BrowserTestUtils.browserLoaded(aboutAddonsBrowser);
    }

    return menu;
  }

  await extension.startup();

  info("Run tests in normal mode");
  await runTestContextMenu({
    id,
    customizing: false,
    testContextMenu,
  });

  info("Run tests in customize mode");
  await runTestContextMenu({
    id,
    customizing: true,
    testContextMenu,
  });

  await extension.unload();

  // Opening the about:addons page will reuse an about:blank tab if it was
  // a new tab that was never navigated, otherwise opens in a new tab when
  // the test is triggering the report action (then we navigate it to
  // about:blank). Here we cleanup all the blank tabs lets behind but one
  // (otherwise the window running the test will be closed and the test
  // would be failing with a timeout).
  info("Cleanup about:blank tabs");
  while (gBrowser.visibleTabs.length > 1) {
    is(
      gBrowser.selectedBrowser.currentURI.spec,
      "about:blank",
      "Expect an about:blank tab"
    );
    const promiseRemovedTab = BrowserTestUtils.waitForEvent(
      gBrowser.selectedTab,
      "TabClose"
    );
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    await promiseRemovedTab;
  }
}

/**
 * Tests that built-in buttons see the Pin to Overflow and Remove items in
 * the toolbar context menu and don't see the Pin to Toolbar item, since
 * that's reserved for extension widgets.
 *
 * @returns {Promise}
 */
async function test_no_toolbar_pinning_on_builtin_helper() {
  let menu = await openContextMenu(TOOLBAR_CONTEXT_MENU, "home-button");
  info(`Pin to Overflow and Remove from Toolbar should be visible.`);
  let pinToOverflow = menu.querySelector(".customize-context-moveToPanel");
  let removeFromToolbar = menu.querySelector(
    ".customize-context-removeFromToolbar"
  );
  Assert.ok(!pinToOverflow.hidden, "Pin to Overflow is visible.");
  Assert.ok(!removeFromToolbar.hidden, "Remove from Toolbar is visible.");
  info(`This button should have "Pin to Toolbar" hidden`);
  let pinToToolbar = menu.querySelector(".customize-context-pinToToolbar");
  Assert.ok(pinToToolbar.hidden, "Pin to Overflow is hidden.");
  menu.hidePopup();
}

add_task(async function test_unified_extensions_ui() {
  await browseraction_popup_contextmenu_helper();
  await browseraction_popup_contextmenu_hidden_items_helper();
  await browseraction_popup_image_contextmenu_helper();
  await browseraction_contextmenu_manage_extension_helper();
  await browseraction_contextmenu_remove_extension_helper();
  await test_no_toolbar_pinning_on_builtin_helper();
});

add_task(async function test_report_amoFormEnabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.abuseReport.amoFormEnabled", true]],
  });
  await browseraction_contextmenu_report_extension_helper();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_report_amoFormDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.abuseReport.amoFormEnabled", false]],
  });
  await browseraction_contextmenu_report_extension_helper();
  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that if Unified Extensions is enabled, that browser actions can
 * be unpinned from the toolbar to the addons panel and back again, via
 * a context menu item.
 */
add_task(async function test_unified_extensions_toolbar_pinning() {
  let id = "addon_id@example.com";
  let nodeId = `${makeWidgetId(id)}-browser-action`;
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: { id },
      },
      browser_action: {
        default_area: "navbar",
      },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  Assert.equal(
    CustomizableUI.getPlacementOfWidget(nodeId).area,
    CustomizableUI.AREA_NAVBAR,
    "Should start placed in the nav-bar."
  );

  let menu = await openContextMenu(TOOLBAR_CONTEXT_MENU, nodeId);

  info(`Pin to Overflow and Remove from Toolbar should be hidden.`);
  let pinToOverflow = menu.querySelector(".customize-context-moveToPanel");
  let removeFromToolbar = menu.querySelector(
    ".customize-context-removeFromToolbar"
  );
  Assert.ok(pinToOverflow.hidden, "Pin to Overflow is hidden.");
  Assert.ok(removeFromToolbar.hidden, "Remove from Toolbar is hidden.");

  info(
    `This button should have "Pin to Toolbar" visible and checked by default.`
  );
  let pinToToolbar = menu.querySelector(".customize-context-pinToToolbar");
  Assert.ok(!pinToToolbar.hidden, "Pin to Toolbar is visible.");
  Assert.equal(
    pinToToolbar.getAttribute("checked"),
    "true",
    "Pin to Toolbar is checked."
  );

  info("Pinning addon to the addons panel.");
  await closeChromeContextMenu(TOOLBAR_CONTEXT_MENU, pinToToolbar);

  Assert.equal(
    CustomizableUI.getPlacementOfWidget(nodeId).area,
    CustomizableUI.AREA_ADDONS,
    "Should have moved the button to the addons panel."
  );

  info("Opening addons panel");
  gUnifiedExtensions.togglePanel();
  await BrowserTestUtils.waitForEvent(gUnifiedExtensions.panel, "popupshown");
  info("Testing unpinning in the addons panel");

  menu = await openContextMenu(UNIFIED_CONTEXT_MENU, nodeId);

  // The UNIFIED_CONTEXT_MENU has a different node for pinToToolbar, so
  // we have to requery for it.
  pinToToolbar = menu.querySelector(
    ".unified-extensions-context-menu-pin-to-toolbar"
  );

  Assert.ok(!pinToToolbar.hidden, "Pin to Toolbar is visible.");
  Assert.equal(
    pinToToolbar.getAttribute("checked"),
    "false",
    "Pin to Toolbar is not checked."
  );
  await closeChromeContextMenu(UNIFIED_CONTEXT_MENU, pinToToolbar);

  Assert.equal(
    CustomizableUI.getPlacementOfWidget(nodeId).area,
    CustomizableUI.AREA_NAVBAR,
    "Should have moved the button back to the nav-bar."
  );

  await extension.unload();
});

/**
 * Tests that there's no Pin to Toolbar option for unified-extensions-item's
 * in the add-ons panel, since these do not represent browser action buttons.
 */
add_task(async function test_unified_extensions_item_no_pinning() {
  let id = "addon_id@example.com";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: { id },
      },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  info("Opening addons panel");
  let panel = gUnifiedExtensions.panel;
  await openExtensionsPanel();

  let items = panel.querySelectorAll("unified-extensions-item");
  Assert.ok(
    !!items.length,
    "There should be at least one unified-extensions-item."
  );

  let menu = await openChromeContextMenu(
    UNIFIED_CONTEXT_MENU,
    `unified-extensions-item[extension-id='${id}']`
  );
  let pinToToolbar = menu.querySelector(
    ".unified-extensions-context-menu-pin-to-toolbar"
  );
  Assert.ok(pinToToolbar.hidden, "Pin to Toolbar is hidden.");
  menu.hidePopup();

  await extension.unload();
});
