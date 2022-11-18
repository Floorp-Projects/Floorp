/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

/* import-globals-from ../../../../../toolkit/mozapps/extensions/test/browser/head.js */

// This is needed to import the `MockProvider`.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/mozapps/extensions/test/browser/head.js",
  this
);

const { ExtensionPermissions } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPermissions.jsm"
);

loadTestSubscript("head_unified_extensions.js");

const openCustomizationUI = async win => {
  const customizationReady = BrowserTestUtils.waitForEvent(
    win.gNavToolbox,
    "customizationready"
  );
  win.gCustomizeMode.enter();
  await customizationReady;
  ok(
    win.CustomizationHandler.isCustomizing(),
    "expected customizing mode to be enabled"
  );
};

const closeCustomizationUI = async win => {
  const afterCustomization = BrowserTestUtils.waitForEvent(
    win.gNavToolbox,
    "aftercustomization"
  );
  win.gCustomizeMode.exit();
  await afterCustomization;
  ok(
    !win.CustomizationHandler.isCustomizing(),
    "expected customizing mode to be disabled"
  );
};

let win;

add_setup(async function() {
  win = await promiseEnableUnifiedExtensions();

  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
  });
});

add_task(async function test_button_enabled_by_pref() {
  const { button } = win.gUnifiedExtensions;
  is(button.hidden, false, "expected button to be visible");
  is(
    win.document
      .getElementById("nav-bar")
      .getAttribute("unifiedextensionsbuttonshown"),
    "true",
    "expected attribute on nav-bar"
  );
});

add_task(async function test_button_disabled_by_pref() {
  const anotherWindow = await promiseDisableUnifiedExtensions();

  const button = anotherWindow.document.getElementById(
    "unified-extensions-button"
  );
  is(button.hidden, true, "expected button to be hidden");
  ok(
    !anotherWindow.document
      .getElementById("nav-bar")
      .hasAttribute("unifiedextensionsbuttonshown"),
    "expected no attribute on nav-bar"
  );

  await BrowserTestUtils.closeWindow(anotherWindow);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_open_panel_on_button_click() {
  const extensions = createExtensions([
    { name: "Extension #1" },
    { name: "Another extension", icons: { 16: "test-icon-16.png" } },
    {
      name: "Yet another extension with an icon",
      icons: {
        32: "test-icon-32.png",
      },
    },
  ]);
  await Promise.all(extensions.map(extension => extension.startup()));

  await openExtensionsPanel(win);

  let item = getUnifiedExtensionsItem(win, extensions[0].id);
  is(
    item.querySelector(".unified-extensions-item-name").textContent,
    "Extension #1",
    "expected name of the first extension"
  );
  is(
    item.querySelector(".unified-extensions-item-icon").src,
    "chrome://mozapps/skin/extensions/extensionGeneric.svg",
    "expected generic icon for the first extension"
  );
  Assert.deepEqual(
    win.document.l10n.getAttributes(
      item.querySelector(".unified-extensions-item-open-menu")
    ),
    {
      id: "unified-extensions-item-open-menu",
      args: { extensionName: "Extension #1" },
    },
    "expected l10n attributes for the first extension"
  );

  item = getUnifiedExtensionsItem(win, extensions[1].id);
  is(
    item.querySelector(".unified-extensions-item-name").textContent,
    "Another extension",
    "expected name of the second extension"
  );
  ok(
    item
      .querySelector(".unified-extensions-item-icon")
      .src.endsWith("/test-icon-16.png"),
    "expected custom icon for the second extension"
  );
  Assert.deepEqual(
    win.document.l10n.getAttributes(
      item.querySelector(".unified-extensions-item-open-menu")
    ),
    {
      id: "unified-extensions-item-open-menu",
      args: { extensionName: "Another extension" },
    },
    "expected l10n attributes for the second extension"
  );

  item = getUnifiedExtensionsItem(win, extensions[2].id);
  is(
    item.querySelector(".unified-extensions-item-name").textContent,
    "Yet another extension with an icon",
    "expected name of the third extension"
  );
  ok(
    item
      .querySelector(".unified-extensions-item-icon")
      .src.endsWith("/test-icon-32.png"),
    "expected custom icon for the third extension"
  );
  Assert.deepEqual(
    win.document.l10n.getAttributes(
      item.querySelector(".unified-extensions-item-open-menu")
    ),
    {
      id: "unified-extensions-item-open-menu",
      args: { extensionName: "Yet another extension with an icon" },
    },
    "expected l10n attributes for the third extension"
  );

  await closeExtensionsPanel(win);

  await Promise.all(extensions.map(extension => extension.unload()));
});

// Verify that the context click doesn't open the panel in addition to the
// context menu.
add_task(async function test_clicks_on_unified_extension_button() {
  const extensions = createExtensions([{ name: "Extension #1" }]);
  await Promise.all(extensions.map(extension => extension.startup()));

  const { button, panel } = win.gUnifiedExtensions;
  ok(button, "expected button");
  ok(panel, "expected panel");

  info("open panel with primary click");
  await openExtensionsPanel(win);
  ok(
    panel.getAttribute("panelopen") === "true",
    "expected panel to be visible"
  );
  await closeExtensionsPanel(win);
  ok(!panel.hasAttribute("panelopen"), "expected panel to be hidden");

  info("open context menu with non-primary click");
  const contextMenu = win.document.getElementById("toolbar-context-menu");
  const popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(
    button,
    { type: "contextmenu", button: 2 },
    win
  );
  await popupShownPromise;
  ok(!panel.hasAttribute("panelopen"), "expected panel to remain hidden");
  await closeChromeContextMenu(contextMenu.id, null, win);

  await Promise.all(extensions.map(extension => extension.unload()));
});

add_task(async function test_item_shows_the_best_addon_icon() {
  const extensions = createExtensions([
    {
      name: "Extension with different icons",
      icons: {
        16: "test-icon-16.png",
        32: "test-icon-32.png",
        64: "test-icon-64.png",
        96: "test-icon-96.png",
        128: "test-icon-128.png",
      },
    },
  ]);
  await Promise.all(extensions.map(extension => extension.startup()));

  for (const { resolution, expectedIcon } of [
    { resolution: 2, expectedIcon: "test-icon-64.png" },
    { resolution: 1, expectedIcon: "test-icon-32.png" },
  ]) {
    await SpecialPowers.pushPrefEnv({
      set: [["layout.css.devPixelsPerPx", String(resolution)]],
    });
    is(
      win.window.devicePixelRatio,
      resolution,
      "window has the required resolution"
    );

    await openExtensionsPanel(win);

    const item = getUnifiedExtensionsItem(win, extensions[0].id);
    const iconSrc = item.querySelector(".unified-extensions-item-icon").src;
    ok(
      iconSrc.endsWith(expectedIcon),
      `expected ${expectedIcon}, got: ${iconSrc}`
    );

    await closeExtensionsPanel(win);
    await SpecialPowers.popPrefEnv();
  }

  await Promise.all(extensions.map(extension => extension.unload()));
});

add_task(async function test_panel_has_a_manage_extensions_button() {
  // Navigate away from the initial page so that about:addons always opens in a
  // new tab during tests.
  BrowserTestUtils.loadURI(win.gBrowser.selectedBrowser, "about:robots");
  await BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);

  await openExtensionsPanel(win);

  const manageExtensionsButton = getListView(win).querySelector(
    "#unified-extensions-manage-extensions"
  );
  ok(manageExtensionsButton, "expected a 'manage extensions' button");

  const tabPromise = BrowserTestUtils.waitForNewTab(
    win.gBrowser,
    "about:addons",
    true
  );
  const popupHiddenPromise = BrowserTestUtils.waitForEvent(
    win.document,
    "popuphidden",
    true
  );

  manageExtensionsButton.click();

  const [tab] = await Promise.all([tabPromise, popupHiddenPromise]);
  is(
    win.gBrowser.currentURI.spec,
    "about:addons",
    "Manage opened about:addons"
  );
  is(
    win.gBrowser.selectedBrowser.contentWindow.gViewController.currentViewId,
    "addons://list/extension",
    "expected about:addons to show the list of extensions"
  );
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_list_active_extensions_only() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.manifestV3.enabled", true]],
  });

  const arrayOfManifestData = [
    { name: "hidden addon", hidden: true },
    { name: "regular addon", hidden: false },
    { name: "disabled addon", hidden: false },
    {
      name: "regular addon with browser action",
      hidden: false,
      browser_action: {
        default_area: "navbar",
      },
    },
    {
      manifest_version: 3,
      name: "regular mv3 addon with browser action",
      hidden: false,
      action: {
        default_area: "navbar",
      },
    },
    {
      name: "regular addon with page action",
      hidden: false,
      page_action: {},
    },
  ];
  const extensions = createExtensions(arrayOfManifestData, {
    // We have to use the mock provider below so we don't need to use the
    // `addonManager` here.
    useAddonManager: false,
  });

  await Promise.all(extensions.map(extension => extension.startup()));

  // We use MockProvider because the "hidden" property cannot be set when
  // "useAddonManager" is passed to loadExtension.
  const mockProvider = new MockProvider();
  mockProvider.createAddons([
    {
      id: extensions[0].id,
      name: arrayOfManifestData[0].name,
      type: "extension",
      version: "1",
      hidden: arrayOfManifestData[0].hidden,
    },
    {
      id: extensions[1].id,
      name: arrayOfManifestData[1].name,
      type: "extension",
      version: "1",
      hidden: arrayOfManifestData[1].hidden,
    },
    {
      id: extensions[2].id,
      name: arrayOfManifestData[2].name,
      type: "extension",
      version: "1",
      hidden: arrayOfManifestData[2].hidden,
      // Mark this add-on as disabled.
      userDisabled: true,
    },
    {
      id: extensions[3].id,
      name: arrayOfManifestData[3].name,
      type: "extension",
      version: "1",
      hidden: arrayOfManifestData[3].hidden,
    },
    {
      id: extensions[4].id,
      name: arrayOfManifestData[4].name,
      type: "extension",
      version: "1",
      hidden: arrayOfManifestData[4].hidden,
    },
    {
      id: extensions[5].id,
      name: arrayOfManifestData[5].name,
      type: "extension",
      version: "1",
      hidden: arrayOfManifestData[5].hidden,
    },
  ]);

  await openExtensionsPanel(win);

  const hiddenAddonItem = getUnifiedExtensionsItem(win, extensions[0].id);
  is(hiddenAddonItem, null, `didn't expect an item for ${extensions[0].id}`);

  const regularAddonItem = getUnifiedExtensionsItem(win, extensions[1].id);
  is(
    regularAddonItem.querySelector(".unified-extensions-item-name").textContent,
    "regular addon",
    "expected an item for a regular add-on"
  );

  const disabledAddonItem = getUnifiedExtensionsItem(win, extensions[2].id);
  is(disabledAddonItem, null, `didn't expect an item for ${extensions[2].id}`);

  const browserActionItem = getUnifiedExtensionsItem(win, extensions[3].id);
  is(browserActionItem, null, `didn't expect an item for ${extensions[3].id}`);

  const mv3BrowserActionItem = getUnifiedExtensionsItem(win, extensions[4].id);
  is(
    mv3BrowserActionItem,
    null,
    `didn't expect an item for ${extensions[4].id}`
  );

  const pageActionItem = getUnifiedExtensionsItem(win, extensions[5].id);
  is(
    pageActionItem.querySelector(".unified-extensions-item-name").textContent,
    "regular addon with page action",
    "expected an item for a regular add-on with page action"
  );

  await closeExtensionsPanel(win);

  await Promise.all(extensions.map(extension => extension.unload()));
  mockProvider.unregister();
});

add_task(async function test_no_addons_themes_widget_when_pref_is_enabled() {
  if (
    !Services.prefs.getBoolPref("extensions.unifiedExtensions.enabled", false)
  ) {
    ok(true, "Skip task because unifiedExtensions pref is disabled");
    return;
  }

  const addonsAndThemesWidgetId = "add-ons-button";

  // Add the button to the navbar, which should not do anything because the
  // add-ons and themes button should not exist when the unified extensions
  // pref is enabled.
  CustomizableUI.addWidgetToArea(
    addonsAndThemesWidgetId,
    CustomizableUI.AREA_NAVBAR
  );

  let addonsButton = win.document.getElementById(addonsAndThemesWidgetId);
  is(addonsButton, null, "expected no add-ons and themes button");
});

add_task(async function test_button_opens_discopane_when_no_extension() {
  // The test harness registers regular extensions so we need to mock the
  // `getActiveExtensions` extension to simulate zero extensions installed.
  const origGetActionExtensions = win.gUnifiedExtensions.getActiveExtensions;
  win.gUnifiedExtensions.getActiveExtensions = () => Promise.resolve([]);

  // Navigate away from the initial page so that about:addons always opens in a
  // new tab during tests.
  BrowserTestUtils.loadURI(win.gBrowser.selectedBrowser, "about:robots");
  await BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);

  const { button } = win.gUnifiedExtensions;
  ok(button, "expected button");

  // Primary click should open about:addons.
  const tabPromise = BrowserTestUtils.waitForNewTab(
    win.gBrowser,
    "about:addons",
    true
  );

  button.click();

  const tab = await tabPromise;
  is(
    win.gBrowser.currentURI.spec,
    "about:addons",
    "expected about:addons to be open"
  );
  is(
    win.gBrowser.selectedBrowser.contentWindow.gViewController.currentViewId,
    "addons://discover/",
    "expected about:addons to show the recommendations"
  );
  BrowserTestUtils.removeTab(tab);

  // "Right-click" should open the context menu only.
  const contextMenu = win.document.getElementById("toolbar-context-menu");
  const popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(
    button,
    { type: "contextmenu", button: 2 },
    win
  );
  await popupShownPromise;
  await closeChromeContextMenu(contextMenu.id, null, win);

  win.gUnifiedExtensions.getActiveExtensions = origGetActionExtensions;
});

add_task(
  async function test_unified_extensions_panel_not_open_in_customization_mode() {
    const listView = getListView(win);
    ok(listView, "expected list view");
    const throwIfExecuted = () => {
      throw new Error("panel should not have been shown");
    };
    listView.addEventListener("ViewShown", throwIfExecuted);

    await openCustomizationUI(win);

    const unifiedExtensionsButtonToggled = BrowserTestUtils.waitForEvent(
      win,
      "UnifiedExtensionsTogglePanel"
    );
    const button = win.document.getElementById("unified-extensions-button");

    button.click();
    await unifiedExtensionsButtonToggled;

    await closeCustomizationUI(win);

    listView.removeEventListener("ViewShown", throwIfExecuted);
  }
);

add_task(async function test_messages_origin_controls() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.manifestV3.enabled", true]],
  });

  const NO_ACCESS = { id: "origin-controls-state-no-access", args: null };
  const ALWAYS_ON = { id: "origin-controls-state-always-on", args: null };
  const WHEN_CLICKED = { id: "origin-controls-state-when-clicked", args: null };
  const HOVER_RUN_VISIT_ONLY = {
    id: "origin-controls-state-hover-run-visit-only",
    args: null,
  };
  const HOVER_RUNNABLE_RUN_EXT = {
    id: "origin-controls-state-runnable-hover-run",
    args: null,
  };
  const HOVER_RUNNABLE_OPEN_EXT = {
    id: "origin-controls-state-runnable-hover-open",
    args: null,
  };

  const TEST_CASES = [
    {
      title: "MV2 - no access",
      manifest: {
        manifest_version: 2,
      },
      expectedDefaultMessage: NO_ACCESS,
      expectedHoverMessage: NO_ACCESS,
      expectedActionButtonDisabled: true,
    },
    {
      title: "MV2 - always on",
      manifest: {
        manifest_version: 2,
        host_permissions: ["*://example.com/*"],
      },
      expectedDefaultMessage: ALWAYS_ON,
      expectedHoverMessage: ALWAYS_ON,
      expectedActionButtonDisabled: true,
    },
    {
      title: "MV2 - content script",
      manifest: {
        manifest_version: 2,
        content_scripts: [
          {
            js: ["script.js"],
            matches: ["*://example.com/*"],
          },
        ],
      },
      expectedDefaultMessage: ALWAYS_ON,
      expectedHoverMessage: ALWAYS_ON,
      expectedActionButtonDisabled: true,
    },
    {
      title: "MV2 - non-matching content script",
      manifest: {
        manifest_version: 2,
        content_scripts: [
          {
            js: ["script.js"],
            matches: ["*://foobar.net/*"],
          },
        ],
      },
      expectedDefaultMessage: NO_ACCESS,
      expectedHoverMessage: NO_ACCESS,
      expectedActionButtonDisabled: true,
    },
    {
      title: "MV2 - all_urls content script",
      manifest: {
        manifest_version: 2,
        content_scripts: [
          {
            js: ["script.js"],
            matches: ["<all_urls>"],
          },
        ],
      },
      expectedDefaultMessage: ALWAYS_ON,
      expectedHoverMessage: ALWAYS_ON,
      expectedActionButtonDisabled: true,
    },
    {
      title: "MV2 - when clicked",
      manifest: {
        manifest_version: 2,
        permissions: ["activeTab"],
      },
      expectedDefaultMessage: WHEN_CLICKED,
      expectedHoverMessage: HOVER_RUN_VISIT_ONLY,
      expectedActionButtonDisabled: false,
    },
    {
      title: "MV2 - browser action - click event - always on",
      manifest: {
        manifest_version: 2,
        browser_action: {},
        host_permissions: ["*://example.com/*"],
      },
      expectedDefaultMessage: ALWAYS_ON,
      expectedHoverMessage: HOVER_RUNNABLE_RUN_EXT,
      expectedActionButtonDisabled: false,
      // TODO: Bug 1799694 - remove this when we set minHeight on CUI widgets
      skipMinHeightCheck: true,
    },
    {
      title: "MV2 - browser action - popup - always on",
      manifest: {
        manifest_version: 2,
        browser_action: {
          default_popup: "popup.html",
        },
        host_permissions: ["*://example.com/*"],
      },
      expectedDefaultMessage: ALWAYS_ON,
      expectedHoverMessage: HOVER_RUNNABLE_OPEN_EXT,
      expectedActionButtonDisabled: false,
      // TODO: Bug 1799694 - remove this when we set minHeight on CUI widgets
      skipMinHeightCheck: true,
    },
    {
      title: "MV2 - browser action - click event - content script",
      manifest: {
        manifest_version: 2,
        browser_action: {},
        content_scripts: [
          {
            js: ["script.js"],
            matches: ["*://example.com/*"],
          },
        ],
      },
      expectedDefaultMessage: ALWAYS_ON,
      expectedHoverMessage: HOVER_RUNNABLE_RUN_EXT,
      expectedActionButtonDisabled: false,
      // TODO: Bug 1799694 - remove this when we set minHeight on CUI widgets
      skipMinHeightCheck: true,
    },
    {
      title: "MV2 - browser action - popup - content script",
      manifest: {
        manifest_version: 2,
        browser_action: {
          default_popup: "popup.html",
        },
        content_scripts: [
          {
            js: ["script.js"],
            matches: ["*://example.com/*"],
          },
        ],
      },
      expectedDefaultMessage: ALWAYS_ON,
      expectedHoverMessage: HOVER_RUNNABLE_OPEN_EXT,
      expectedActionButtonDisabled: false,
      // TODO: Bug 1799694 - remove this when we set minHeight on CUI widgets
      skipMinHeightCheck: true,
    },
    {
      title: "no access",
      manifest: {
        manifest_version: 3,
      },
      expectedDefaultMessage: NO_ACCESS,
      expectedHoverMessage: NO_ACCESS,
      expectedActionButtonDisabled: true,
    },
    {
      title: "when clicked with host permissions",
      manifest: {
        manifest_version: 3,
        host_permissions: ["*://example.com/*"],
      },
      expectedDefaultMessage: WHEN_CLICKED,
      expectedHoverMessage: HOVER_RUN_VISIT_ONLY,
      expectedActionButtonDisabled: false,
    },
    {
      title: "when clicked with host permissions already granted",
      manifest: {
        manifest_version: 3,
        host_permissions: ["*://example.com/*"],
      },
      expectedDefaultMessage: ALWAYS_ON,
      expectedHoverMessage: ALWAYS_ON,
      expectedActionButtonDisabled: true,
      grantHostPermissions: true,
    },
    {
      title: "when clicked",
      manifest: {
        manifest_version: 3,
        permissions: ["activeTab"],
      },
      expectedDefaultMessage: WHEN_CLICKED,
      expectedHoverMessage: HOVER_RUN_VISIT_ONLY,
      expectedActionButtonDisabled: false,
    },
    {
      title: "page action - no access",
      manifest: {
        manifest_version: 3,
        page_action: {},
      },
      expectedDefaultMessage: NO_ACCESS,
      expectedHoverMessage: NO_ACCESS,
      expectedActionButtonDisabled: true,
    },
    {
      title: "page action - when clicked with host permissions",
      manifest: {
        manifest_version: 3,
        host_permissions: ["*://example.com/*"],
        page_action: {},
      },
      expectedDefaultMessage: WHEN_CLICKED,
      expectedHoverMessage: HOVER_RUN_VISIT_ONLY,
      expectedActionButtonDisabled: false,
    },
    {
      title: "page action - when clicked with host permissions already granted",
      manifest: {
        manifest_version: 3,
        host_permissions: ["*://example.com/*"],
        page_action: {},
      },
      expectedDefaultMessage: ALWAYS_ON,
      expectedHoverMessage: ALWAYS_ON,
      expectedActionButtonDisabled: true,
      grantHostPermissions: true,
    },
    {
      title: "page action - when clicked",
      manifest: {
        manifest_version: 3,
        permissions: ["activeTab"],
        page_action: {},
      },
      expectedDefaultMessage: WHEN_CLICKED,
      expectedHoverMessage: HOVER_RUN_VISIT_ONLY,
      expectedActionButtonDisabled: false,
    },
    {
      title: "browser action - click event - no access",
      manifest: {
        manifest_version: 3,
        action: {},
      },
      expectedDefaultMessage: NO_ACCESS,
      expectedHoverMessage: HOVER_RUNNABLE_RUN_EXT,
      expectedActionButtonDisabled: false,
      // TODO: Bug 1799694 - remove this when we set minHeight on CUI widgets
      skipMinHeightCheck: true,
    },
    {
      title: "browser action - popup - no access",
      manifest: {
        manifest_version: 3,
        action: {
          default_popup: "popup.html",
        },
      },
      expectedDefaultMessage: NO_ACCESS,
      expectedHoverMessage: HOVER_RUNNABLE_OPEN_EXT,
      expectedActionButtonDisabled: false,
      // TODO: Bug 1799694 - remove this when we set minHeight on CUI widgets
      skipMinHeightCheck: true,
    },
    {
      title: "browser action - click event - when clicked",
      manifest: {
        manifest_version: 3,
        action: {},
        host_permissions: ["*://example.com/*"],
      },
      expectedDefaultMessage: WHEN_CLICKED,
      expectedHoverMessage: HOVER_RUN_VISIT_ONLY,
      expectedActionButtonDisabled: false,
      // TODO: Bug 1799694 - remove this when we set minHeight on CUI widgets
      skipMinHeightCheck: true,
    },
    {
      title: "browser action - popup - when clicked",
      manifest: {
        manifest_version: 3,
        action: {
          default_popup: "popup.html",
        },
        host_permissions: ["*://example.com/*"],
      },
      expectedDefaultMessage: WHEN_CLICKED,
      expectedHoverMessage: HOVER_RUN_VISIT_ONLY,
      expectedActionButtonDisabled: false,
      // TODO: Bug 1799694 - remove this when we set minHeight on CUI widgets
      skipMinHeightCheck: true,
    },
    {
      title:
        "browser action - click event - when clicked with host permissions already granted",
      manifest: {
        manifest_version: 3,
        action: {},
        host_permissions: ["*://example.com/*"],
      },
      expectedDefaultMessage: ALWAYS_ON,
      expectedHoverMessage: HOVER_RUNNABLE_RUN_EXT,
      expectedActionButtonDisabled: false,
      grantHostPermissions: true,
      // TODO: Bug 1799694 - remove this when we set minHeight on CUI widgets
      skipMinHeightCheck: true,
    },
    {
      title:
        "browser action - popup - when clicked with host permissions already granted",
      manifest: {
        manifest_version: 3,
        action: {
          default_popup: "popup.html",
        },
        host_permissions: ["*://example.com/*"],
      },
      expectedDefaultMessage: ALWAYS_ON,
      expectedHoverMessage: HOVER_RUNNABLE_OPEN_EXT,
      expectedActionButtonDisabled: false,
      grantHostPermissions: true,
      // TODO: Bug 1799694 - remove this when we set minHeight on CUI widgets
      skipMinHeightCheck: true,
    },
  ];

  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url: "https://example.com/" },
    async () => {
      let count = 0;

      for (const {
        title,
        manifest,
        expectedDefaultMessage,
        expectedHoverMessage,
        expectedActionButtonDisabled,
        grantHostPermissions,
        // TODO: Bug 1799694 - remove this when we set minHeight on CUI widgets
        skipMinHeightCheck,
      } of TEST_CASES) {
        info(`case: ${title}`);

        const id = `test-origin-controls-${count++}@ext`;
        const extension = ExtensionTestUtils.loadExtension({
          manifest: {
            name: title,
            browser_specific_settings: { gecko: { id } },
            ...manifest,
          },
          files: {
            "script.js": "",
            "popup.html": "",
          },
          useAddonManager: "temporary",
        });

        if (grantHostPermissions) {
          info("Granting initial permissions.");
          await ExtensionPermissions.add(id, {
            permissions: [],
            origins: manifest.host_permissions,
          });
        }

        await extension.startup();

        // Open the extension panel.
        await openExtensionsPanel(win);

        const item = getUnifiedExtensionsItem(win, extension.id);
        ok(item, `expected item for ${extension.id}`);

        // 1. Verify the default message displayed below the extension's name.
        const message = item.querySelector(
          ".unified-extensions-item-message-default"
        );
        ok(message, "expected a default message element");

        Assert.deepEqual(
          win.document.l10n.getAttributes(message),
          expectedDefaultMessage,
          "expected l10n attributes for the default message"
        );

        const minHeight = item.querySelector(
          ".unified-extensions-item-contents"
        )?.style?.minHeight;

        // TODO: Bug 1799694 - remove this when we set minHeight on CUI widgets
        if (!skipMinHeightCheck) {
          // 2. Verify that a min-height has been set on the message's parent
          // element.
          ok(!!minHeight, "expected min-height to be set");
        }

        // 3. Verify the action button state.
        const actionButton = item.querySelector(
          ".unified-extensions-item-action"
        );
        ok(actionButton, "expected an action button");
        is(
          actionButton.disabled,
          expectedActionButtonDisabled,
          `expected action button to be ${
            expectedActionButtonDisabled ? "disabled" : "enabled"
          }`
        );

        // 4. Verify the message displayed on hover but only when the action
        // button isn't disabled to avoid some test failures.
        if (!expectedActionButtonDisabled) {
          const hovered = BrowserTestUtils.waitForEvent(
            actionButton,
            "mouseover"
          );
          EventUtils.synthesizeMouseAtCenter(
            actionButton,
            { type: "mouseover" },
            win
          );
          await hovered;

          Assert.deepEqual(
            win.document.l10n.getAttributes(message),
            expectedHoverMessage,
            "expected l10n attributes for the message on hover"
          );
        }

        // TODO: Bug 1799694 - remove this when we set minHeight on CUI widgets
        if (!skipMinHeightCheck) {
          // 5. Min-height shouldn't change
          is(
            item.querySelector(".unified-extensions-item-contents")?.style
              ?.minHeight,
            minHeight,
            "expected same min-height as earlier"
          );
        }

        await closeExtensionsPanel(win);

        // Move cursor elsewhere to avoid issues with previous "hovering".
        EventUtils.synthesizeMouseAtCenter(win.gURLBar.textbox, {}, win);

        await extension.unload();
      }
    }
  );

  await SpecialPowers.popPrefEnv();
});
