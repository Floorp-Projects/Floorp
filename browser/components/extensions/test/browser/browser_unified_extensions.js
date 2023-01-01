/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

/* import-globals-from ../../../../../toolkit/mozapps/extensions/test/browser/head.js */

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

  // Make sure extension buttons added to the navbar will not overflow in the
  // panel, which could happen when a previous test file resizes the current
  // window.
  await ensureMaximizedWindow(win);

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
      item.querySelector(".unified-extensions-item-menu-button")
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
      item.querySelector(".unified-extensions-item-menu-button")
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
      item.querySelector(".unified-extensions-item-menu-button")
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

  // On MacOS, ctrl-click shouldn't open the panel because this normally opens
  // the context menu. We can't test anything on MacOS...
  if (AppConstants.platform !== "macosx") {
    info("open panel with ctrl-click");
    const listView = getListView(win);
    const viewShown = BrowserTestUtils.waitForEvent(listView, "ViewShown");
    EventUtils.synthesizeMouseAtCenter(button, { ctrlKey: true }, win);
    await viewShown;
    ok(
      panel.getAttribute("panelopen") === "true",
      "expected panel to be visible"
    );
    await closeExtensionsPanel(win);
    ok(!panel.hasAttribute("panelopen"), "expected panel to be hidden");
  }

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
    {
      name: "hidden addon",
      browser_specific_settings: { gecko: { id: "ext1@test" } },
      hidden: true,
    },
    {
      name: "regular addon",
      browser_specific_settings: { gecko: { id: "ext2@test" } },
      hidden: false,
    },
    {
      name: "disabled addon",
      browser_specific_settings: { gecko: { id: "ext3@test" } },
      hidden: false,
    },
    {
      name: "regular addon with browser action",
      browser_specific_settings: { gecko: { id: "ext4@test" } },
      hidden: false,
      browser_action: {
        default_area: "navbar",
      },
    },
    {
      manifest_version: 3,
      name: "regular mv3 addon with browser action",
      browser_specific_settings: { gecko: { id: "ext5@test" } },
      hidden: false,
      action: {
        default_area: "navbar",
      },
    },
    {
      name: "regular addon with page action",
      browser_specific_settings: { gecko: { id: "ext6@test" } },
      hidden: false,
      page_action: {},
    },
  ];
  const extensions = createExtensions(arrayOfManifestData, {
    useAddonManager: "temporary",
    // Allow all extensions in PB mode by default.
    incognitoOverride: "spanning",
  });
  // This extension is loaded with a different `incognitoOverride` value to
  // make sure it won't show up in a private window.
  extensions.push(
    ExtensionTestUtils.loadExtension({
      manifest: {
        browser_specific_settings: { gecko: { id: "ext7@test" } },
        name: "regular addon with private browsing disabled",
      },
      useAddonManager: "temporary",
      incognitoOverride: "not_allowed",
    })
  );

  await Promise.all(extensions.map(extension => extension.startup()));

  // Disable the "disabled addon".
  let addon2 = await AddonManager.getAddonByID(extensions[2].id);
  await addon2.disable();

  for (const isPrivate of [false, true]) {
    info(
      `verifying extensions listed in the panel with private browsing ${
        isPrivate ? "enabled" : "disabled"
      }`
    );
    const aWin = await promiseEnableUnifiedExtensions({ private: isPrivate });
    // Make sure extension buttons added to the navbar will not overflow in the
    // panel, which could happen when a previous test file resizes the current
    // window.
    await ensureMaximizedWindow(aWin);

    await openExtensionsPanel(aWin);

    const hiddenAddonItem = getUnifiedExtensionsItem(aWin, extensions[0].id);
    is(hiddenAddonItem, null, "didn't expect an item for a hidden add-on");

    const regularAddonItem = getUnifiedExtensionsItem(aWin, extensions[1].id);
    is(
      regularAddonItem.querySelector(".unified-extensions-item-name")
        .textContent,
      "regular addon",
      "expected an item for a regular add-on"
    );

    const disabledAddonItem = getUnifiedExtensionsItem(aWin, extensions[2].id);
    is(disabledAddonItem, null, "didn't expect an item for a disabled add-on");

    const browserActionItem = getUnifiedExtensionsItem(aWin, extensions[3].id);
    is(
      browserActionItem,
      null,
      "didn't expect an item for an add-on with browser action placed in the navbar"
    );

    const mv3BrowserActionItem = getUnifiedExtensionsItem(
      aWin,
      extensions[4].id
    );
    is(
      mv3BrowserActionItem,
      null,
      "didn't expect an item for a MV3 add-on with browser action placed in the navbar"
    );

    const pageActionItem = getUnifiedExtensionsItem(aWin, extensions[5].id);
    is(
      pageActionItem.querySelector(".unified-extensions-item-name").textContent,
      "regular addon with page action",
      "expected an item for a regular add-on with page action"
    );

    const privateBrowsingDisabledItem = getUnifiedExtensionsItem(
      aWin,
      extensions[6].id
    );
    if (isPrivate) {
      is(
        privateBrowsingDisabledItem,
        null,
        "didn't expect an item for a regular add-on with private browsing enabled"
      );
    } else {
      is(
        privateBrowsingDisabledItem.querySelector(
          ".unified-extensions-item-name"
        ).textContent,
        "regular addon with private browsing disabled",
        "expected an item for a regular add-on with private browsing disabled"
      );
    }

    await closeExtensionsPanel(aWin);

    await BrowserTestUtils.closeWindow(aWin);
  }

  await Promise.all(extensions.map(extension => extension.unload()));
});

add_task(async function test_button_opens_discopane_when_no_extension() {
  // The test harness registers regular extensions so we need to mock the
  // `getActivePolicies` extension to simulate zero extensions installed.
  const origGetActivePolicies = win.gUnifiedExtensions.getActivePolicies;
  win.gUnifiedExtensions.getActivePolicies = () => [];

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

  win.gUnifiedExtensions.getActivePolicies = origGetActivePolicies;
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

const NO_ACCESS = { id: "origin-controls-state-no-access", args: null };
const ALWAYS_ON = { id: "origin-controls-state-always-on", args: null };
const WHEN_CLICKED = { id: "origin-controls-state-when-clicked", args: null };
const TEMP_ACCESS = {
  id: "origin-controls-state-temporary-access",
  args: null,
};

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

add_task(async function test_messages_origin_controls() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.manifestV3.enabled", true]],
  });

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
      title: "MV2 - activeTab without browser action",
      manifest: {
        manifest_version: 2,
        permissions: ["activeTab"],
      },
      expectedDefaultMessage: NO_ACCESS,
      expectedHoverMessage: NO_ACCESS,
      expectedActionButtonDisabled: true,
    },
    {
      title: "MV2 - when clicked: activeTab with browser action",
      manifest: {
        manifest_version: 2,
        permissions: ["activeTab"],
        browser_action: {},
      },
      expectedDefaultMessage: WHEN_CLICKED,
      expectedHoverMessage: HOVER_RUN_VISIT_ONLY,
      expectedActionButtonDisabled: false,
    },
    {
      title: "MV3 - when clicked: activeTab with action",
      manifest: {
        manifest_version: 3,
        permissions: ["activeTab"],
        action: {},
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

        const messageDeck = item.querySelector(
          ".unified-extensions-item-message-deck"
        );
        ok(messageDeck, "expected a message deck element");

        // 1. Verify the default message displayed below the extension's name.
        const defaultMessage = item.querySelector(
          ".unified-extensions-item-message-default"
        );
        ok(defaultMessage, "expected a default message element");

        Assert.deepEqual(
          win.document.l10n.getAttributes(defaultMessage),
          expectedDefaultMessage,
          "expected l10n attributes for the default message"
        );

        is(
          messageDeck.selectedIndex,
          win.gUnifiedExtensions.MESSAGE_DECK_INDEX_DEFAULT,
          "expected selected message in the deck to be the default message"
        );

        // 2. Verify the action button state.
        const actionButton = item.querySelector(
          ".unified-extensions-item-action-button"
        );
        ok(actionButton, "expected an action button");
        is(
          actionButton.disabled,
          expectedActionButtonDisabled,
          `expected action button to be ${
            expectedActionButtonDisabled ? "disabled" : "enabled"
          }`
        );

        // 3. Verify the message displayed on hover but only when the action
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

          const hoverMessage = item.querySelector(
            ".unified-extensions-item-message-hover"
          );
          ok(hoverMessage, "expected a hover message element");

          Assert.deepEqual(
            win.document.l10n.getAttributes(hoverMessage),
            expectedHoverMessage,
            "expected l10n attributes for the message on hover"
          );

          is(
            messageDeck.selectedIndex,
            win.gUnifiedExtensions.MESSAGE_DECK_INDEX_HOVER,
            "expected selected message in the deck to be the hover message"
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

add_task(async function test_hover_message_when_button_updates_itself() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.manifestV3.enabled", true]],
  });

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      name: "an extension that refreshes its title",
      action: {},
    },
    background() {
      browser.test.onMessage.addListener(async msg => {
        browser.test.assertEq(
          "update-button",
          msg,
          "expected 'update-button' message"
        );

        browser.action.setTitle({ title: "a title" });

        browser.test.sendMessage(`${msg}-done`);
      });

      browser.test.sendMessage("background-ready");
    },
    useAddonManager: "temporary",
  });

  await extension.startup();
  await extension.awaitMessage("background-ready");

  await openExtensionsPanel(win);

  const item = getUnifiedExtensionsItem(win, extension.id);
  ok(item, "expected item in the panel");

  const actionButton = item.querySelector(
    ".unified-extensions-item-action-button"
  );
  ok(actionButton, "expected an action button");

  const menuButton = item.querySelector(".unified-extensions-item-menu-button");
  ok(menuButton, "expected a menu button");

  const hovered = BrowserTestUtils.waitForEvent(actionButton, "mouseover");
  EventUtils.synthesizeMouseAtCenter(actionButton, { type: "mouseover" }, win);
  await hovered;

  const messageDeck = item.querySelector(
    ".unified-extensions-item-message-deck"
  );
  ok(messageDeck, "expected a message deck element");

  const hoverMessage = item.querySelector(
    ".unified-extensions-item-message-hover"
  );
  ok(hoverMessage, "expected a hover message element");

  const expectedL10nAttributes = {
    id: "origin-controls-state-runnable-hover-run",
    args: null,
  };
  Assert.deepEqual(
    win.document.l10n.getAttributes(hoverMessage),
    expectedL10nAttributes,
    "expected l10n attributes for the hover message"
  );

  is(
    messageDeck.selectedIndex,
    win.gUnifiedExtensions.MESSAGE_DECK_INDEX_HOVER,
    "expected selected message in the deck to be the hover message"
  );

  extension.sendMessage("update-button");
  await extension.awaitMessage("update-button-done");

  is(
    messageDeck.selectedIndex,
    win.gUnifiedExtensions.MESSAGE_DECK_INDEX_HOVER,
    "expected selected message in the deck to remain the same"
  );

  const menuButtonHovered = BrowserTestUtils.waitForEvent(
    menuButton,
    "mouseover"
  );
  EventUtils.synthesizeMouseAtCenter(menuButton, { type: "mouseover" }, win);
  await menuButtonHovered;

  await closeExtensionsPanel(win);

  await extension.unload();
});

// Test the temporary access state messages and attention indicator.
add_task(async function test_temporary_access() {
  const TEST_CASES = [
    {
      title: "mv3 with active scripts and browser action",
      manifest: {
        manifest_version: 3,
        action: {},
        content_scripts: [
          {
            js: ["script.js"],
            matches: ["*://example.com/*"],
          },
        ],
      },
      before: {
        attention: true,
        state: WHEN_CLICKED,
        disabled: false,
      },
      messages: ["action-onClicked", "cs-injected"],
      after: {
        attention: false,
        state: TEMP_ACCESS,
        disabled: false,
      },
    },
    {
      title: "mv3 with active scripts and no browser action",
      manifest: {
        manifest_version: 3,
        content_scripts: [
          {
            js: ["script.js"],
            matches: ["*://example.com/*"],
          },
        ],
      },
      before: {
        attention: true,
        state: WHEN_CLICKED,
        disabled: false,
      },
      messages: ["cs-injected"],
      after: {
        attention: false,
        state: TEMP_ACCESS,
        // TODO: This will need updating for bug 1807835.
        disabled: false,
      },
    },
    {
      title: "mv3 with browser action and host_permission",
      manifest: {
        manifest_version: 3,
        action: {},
        host_permissions: ["*://example.com/*"],
      },
      before: {
        attention: true,
        state: WHEN_CLICKED,
        disabled: false,
      },
      messages: ["action-onClicked"],
      after: {
        attention: false,
        state: TEMP_ACCESS,
        disabled: false,
      },
    },
    {
      title: "mv3 with browser action no host_permissions",
      manifest: {
        manifest_version: 3,
        action: {},
      },
      before: {
        attention: false,
        state: NO_ACCESS,
        disabled: false,
      },
      messages: ["action-onClicked"],
      after: {
        attention: false,
        state: NO_ACCESS,
        disabled: false,
      },
    },
    // MV2 tests.
    {
      title: "mv2 with content scripts and browser action",
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
      before: {
        attention: false,
        state: ALWAYS_ON,
        disabled: false,
      },
      messages: ["action-onClicked", "cs-injected"],
      after: {
        attention: false,
        state: ALWAYS_ON,
        disabled: false,
      },
    },
    {
      title: "mv2 with content scripts and no browser action",
      manifest: {
        manifest_version: 2,
        content_scripts: [
          {
            js: ["script.js"],
            matches: ["*://example.com/*"],
          },
        ],
      },
      before: {
        attention: false,
        state: ALWAYS_ON,
        disabled: true,
      },
      messages: ["cs-injected"],
      after: {
        attention: false,
        state: ALWAYS_ON,
        disabled: true,
      },
    },
    {
      title: "mv2 with browser action and host_permission",
      manifest: {
        manifest_version: 2,
        browser_action: {},
        host_permissions: ["*://example.com/*"],
      },
      before: {
        attention: false,
        state: ALWAYS_ON,
        disabled: false,
      },
      messages: ["action-onClicked"],
      after: {
        attention: false,
        state: ALWAYS_ON,
        disabled: false,
      },
    },
    {
      title: "mv2 with browser action no host_permissions",
      manifest: {
        manifest_version: 2,
        browser_action: {},
      },
      before: {
        attention: false,
        state: NO_ACCESS,
        disabled: false,
      },
      messages: ["action-onClicked"],
      after: {
        attention: false,
        state: NO_ACCESS,
        disabled: false,
      },
    },
  ];

  let count = 1;
  await Promise.all(
    TEST_CASES.map(test => {
      let id = `test-temp-access-${count++}@ext`;
      test.extension = ExtensionTestUtils.loadExtension({
        manifest: {
          name: test.title,
          browser_specific_settings: { gecko: { id } },
          ...test.manifest,
        },
        files: {
          "popup.html": "",
          "script.js"() {
            browser.test.sendMessage("cs-injected");
          },
        },
        background() {
          let action = browser.action ?? browser.browserAction;
          action?.onClicked.addListener(() => {
            browser.test.sendMessage("action-onClicked");
          });
        },
        useAddonManager: "temporary",
      });

      return test.extension.startup();
    })
  );

  async function checkButton(extension, expect, click = false) {
    await openExtensionsPanel(win);

    let item = getUnifiedExtensionsItem(win, extension.id);
    ok(item, `Expected item for ${extension.id}.`);

    let state = item.querySelector(".unified-extensions-item-message-default");
    ok(state, "Expected a default state message element.");

    is(
      item.hasAttribute("attention"),
      !!expect.attention,
      "Expected attention badge."
    );
    Assert.deepEqual(
      win.document.l10n.getAttributes(state),
      expect.state,
      "Expected l10n attributes for the message."
    );

    let button = item.querySelector(".unified-extensions-item-action-button");
    is(button.disabled, !!expect.disabled, "Expect disabled item.");

    // If we should click, and button is not disabled.
    if (click && !expect.disabled) {
      let onClick = BrowserTestUtils.waitForEvent(button, "click");
      button.click();
      await onClick;
    } else {
      // Otherwise, just close the panel.
      await closeExtensionsPanel(win);
    }
  }

  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url: "https://example.com/" },
    async () => {
      for (let { title, extension, before, messages, after } of TEST_CASES) {
        info(`Test case: ${title}`);
        await checkButton(extension, before, true);

        await Promise.all(
          messages.map(msg => {
            info(`Waiting for ${msg} from clicking the button.`);
            return extension.awaitMessage(msg);
          })
        );

        await checkButton(extension, after);
        await extension.unload();
      }
    }
  );
});
