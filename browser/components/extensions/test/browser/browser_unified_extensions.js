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
});

add_task(async function test_button_disabled_by_pref() {
  const anotherWindow = await promiseDisableUnifiedExtensions();

  const button = anotherWindow.document.getElementById(
    "unified-extensions-button"
  );
  is(button.hidden, true, "expected button to be hidden");

  await BrowserTestUtils.closeWindow(anotherWindow);
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
  const arrayOfManifestData = [
    { name: "hidden addon", hidden: true },
    { name: "regular addon", hidden: false },
    { name: "disabled addon", hidden: false },
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

  await closeExtensionsPanel(win);

  await Promise.all(extensions.map(extension => extension.unload()));
  mockProvider.unregister();
});

add_task(async function test_unified_extensions_and_addons_themes_widget() {
  const addonsAndThemesWidgetId = "add-ons-button";

  // Let's start with the unified extensions feature is disabled.
  let anotherWindow = await promiseDisableUnifiedExtensions();

  // Add the button to the navbar.
  CustomizableUI.addWidgetToArea(
    addonsAndThemesWidgetId,
    CustomizableUI.AREA_NAVBAR
  );
  let cleanupDone = false;
  const cleanup = () => {
    if (cleanupDone) {
      return;
    }
    cleanupDone = true;

    CustomizableUI.reset();
  };
  registerCleanupFunction(cleanup);

  let addonsButton = anotherWindow.document.getElementById(
    addonsAndThemesWidgetId
  );
  ok(addonsButton, "expected add-ons and themes button");

  await BrowserTestUtils.closeWindow(anotherWindow);
  // Now we enable the unified extensions feature, which should remove the
  // add-ons button.
  anotherWindow = await promiseEnableUnifiedExtensions();

  addonsButton = anotherWindow.document.getElementById(addonsAndThemesWidgetId);
  is(addonsButton, null, "expected no add-ons and themes button");

  await BrowserTestUtils.closeWindow(anotherWindow);
  // Disable the unified extensions feature again. We expect the add-ons and
  // themes button to be back (i.e. visible).
  anotherWindow = await promiseDisableUnifiedExtensions();

  addonsButton = anotherWindow.document.getElementById(addonsAndThemesWidgetId);
  ok(addonsButton, "expected add-ons and themes button");

  cleanup();

  await BrowserTestUtils.closeWindow(anotherWindow);
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
