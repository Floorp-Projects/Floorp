const { AddonManagerPrivate } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs"
);

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);
AddonTestUtils.hookAMTelemetryEvents();

const ID = "update2@tests.mozilla.org";
const ID_ICON = "update_icon2@tests.mozilla.org";
const ID_PERMS = "update_perms@tests.mozilla.org";
const ID_LEGACY = "legacy_update@tests.mozilla.org";
const FAKE_INSTALL_TELEMETRY_SOURCE = "fake-install-source";

requestLongerTimeout(2);

function promiseViewLoaded(tab, viewid) {
  let win = tab.linkedBrowser.contentWindow;
  if (
    win.gViewController &&
    !win.gViewController.isLoading &&
    win.gViewController.currentViewId == viewid
  ) {
    return Promise.resolve();
  }

  return waitAboutAddonsViewLoaded(win.document);
}

function getBadgeStatus() {
  let menuButton = document.getElementById("PanelUI-menu-button");
  return menuButton.getAttribute("badge-status");
}

// Set some prefs that apply to all the tests in this file
add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      // We don't have pre-pinned certificates for the local mochitest server
      ["extensions.install.requireBuiltInCerts", false],
      ["extensions.update.requireBuiltInCerts", false],
    ],
  });

  // Navigate away from the initial page so that about:addons always
  // opens in a new tab during tests
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    "about:robots"
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  registerCleanupFunction(async function () {
    // Return to about:blank when we're done
    BrowserTestUtils.startLoadingURIString(
      gBrowser.selectedBrowser,
      "about:blank"
    );
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  });
});

// Helper function to test background updates.
async function backgroundUpdateTest(url, id, checkIconFn) {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Turn on background updates
      ["extensions.update.enabled", true],

      // Point updates to the local mochitest server
      [
        "extensions.update.background.url",
        `${BASE}/browser_webext_update.json`,
      ],
    ],
  });

  Services.fog.testResetFOG();

  // Install version 1.0 of the test extension
  let addon = await promiseInstallAddon(url, {
    source: FAKE_INSTALL_TELEMETRY_SOURCE,
  });
  let addonId = addon.id;

  ok(addon, "Addon was installed");
  is(getBadgeStatus(), "", "Should not start out with an addon alert badge");

  // Trigger an update check and wait for the update for this addon
  // to be downloaded.
  let updatePromise = promiseInstallEvent(addon, "onDownloadEnded");

  AddonManagerPrivate.backgroundUpdateCheck();
  await updatePromise;

  is(getBadgeStatus(), "addon-alert", "Should have addon alert badge");

  // Find the menu entry for the update
  await gCUITestUtils.openMainMenu();

  let addons = PanelUI.addonNotificationContainer;
  is(addons.children.length, 1, "Have a menu entry for the update");

  // Click the menu item
  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:addons");
  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  addons.children[0].click();

  // The click should hide the main menu. This is currently synchronous.
  ok(PanelUI.panel.state != "open", "Main menu is closed or closing.");

  // about:addons should load and go to the list of extensions
  let tab = await tabPromise;
  is(
    tab.linkedBrowser.currentURI.spec,
    "about:addons",
    "Browser is at about:addons"
  );

  const VIEW = "addons://list/extension";
  await promiseViewLoaded(tab, VIEW);
  let win = tab.linkedBrowser.contentWindow;
  ok(!win.gViewController.isLoading, "about:addons view is fully loaded");
  is(
    win.gViewController.currentViewId,
    VIEW,
    "about:addons is at extensions list"
  );

  // Wait for the permission prompt, check the contents
  let panel = await popupPromise;
  checkIconFn(panel.getAttribute("icon"));

  // The original extension has 1 promptable permission and the new one
  // has 2 (history and <all_urls>) plus 1 non-promptable permission (cookies).
  // So we should only see the 1 new promptable permission in the notification.
  let singlePermissionEl = document.getElementById(
    "addon-webext-perm-single-entry"
  );
  ok(!singlePermissionEl.hidden, "Single permission entry is not hidden");
  ok(singlePermissionEl.textContent, "Single permission entry text is set");

  // Cancel the update.
  panel.secondaryButton.click();

  addon = await AddonManager.getAddonByID(id);
  is(addon.version, "1.0", "Should still be running the old version");

  BrowserTestUtils.removeTab(tab);

  // Alert badge and hamburger menu items should be gone
  is(getBadgeStatus(), "", "Addon alert badge should be gone");

  await gCUITestUtils.openMainMenu();
  addons = PanelUI.addonNotificationContainer;
  is(addons.children.length, 0, "Update menu entries should be gone");
  await gCUITestUtils.hideMainMenu();

  // Re-check for an update
  updatePromise = promiseInstallEvent(addon, "onDownloadEnded");
  await AddonManagerPrivate.backgroundUpdateCheck();
  await updatePromise;

  is(getBadgeStatus(), "addon-alert", "Should have addon alert badge");

  // Find the menu entry for the update
  await gCUITestUtils.openMainMenu();

  addons = PanelUI.addonNotificationContainer;
  is(addons.children.length, 1, "Have a menu entry for the update");

  // Click the menu item
  tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:addons", true);
  popupPromise = promisePopupNotificationShown("addon-webext-permissions");

  addons.children[0].click();

  // Wait for about:addons to load
  tab = await tabPromise;
  is(tab.linkedBrowser.currentURI.spec, "about:addons");

  await promiseViewLoaded(tab, VIEW);
  win = tab.linkedBrowser.contentWindow;
  ok(!win.gViewController.isLoading, "about:addons view is fully loaded");
  is(
    win.gViewController.currentViewId,
    VIEW,
    "about:addons is at extensions list"
  );

  // Wait for the permission prompt and accept it this time
  updatePromise = waitForUpdate(addon);
  panel = await popupPromise;
  panel.button.click();

  addon = await updatePromise;
  is(addon.version, "2.0", "Should have upgraded to the new version");

  BrowserTestUtils.removeTab(tab);

  is(getBadgeStatus(), "", "Addon alert badge should be gone");

  await addon.uninstall();
  await SpecialPowers.popPrefEnv();

  let gleanUpdates = AddonTestUtils.getAMGleanEvents("update");

  // Test that the expected telemetry events have been recorded (and that they include the
  // permission_prompt event).
  const amEvents = AddonTestUtils.getAMTelemetryEvents();
  const updateEvents = amEvents
    .filter(evt => evt.method === "update")
    .map(evt => {
      delete evt.value;
      return evt;
    });

  const expectedSteps = [
    // First update (cancelled).
    "started",
    "download_started",
    "download_completed",
    "permissions_prompt",
    "cancelled",
    // Second update (completed).
    "started",
    "download_started",
    "download_completed",
    "permissions_prompt",
    "completed",
  ];

  Assert.deepEqual(
    expectedSteps,
    updateEvents.map(evt => evt.extra && evt.extra.step),
    "Got the steps from the collected telemetry events"
  );

  Assert.deepEqual(
    expectedSteps,
    gleanUpdates.map(evt => evt.step),
    "Got the steps from the collected Glean events."
  );

  const method = "update";
  const object = "extension";
  const baseExtra = {
    addon_id: addonId,
    source: FAKE_INSTALL_TELEMETRY_SOURCE,
    step: "permissions_prompt",
    updated_from: "app",
  };

  // Expect the telemetry events to have num_strings set to 1, as only the origin permissions is going
  // to be listed in the permission prompt.
  Assert.deepEqual(
    updateEvents.filter(
      evt => evt.extra && evt.extra.step === "permissions_prompt"
    ),
    [
      { method, object, extra: { ...baseExtra, num_strings: "1" } },
      { method, object, extra: { ...baseExtra, num_strings: "1" } },
    ],
    "Got the expected permission_prompts events"
  );

  Assert.deepEqual(
    gleanUpdates.filter(e => e.step === "permissions_prompt"),
    [
      { ...baseExtra, addon_type: object, num_strings: "1" },
      { ...baseExtra, addon_type: object, num_strings: "1" },
    ],
    "Got the expected permission_prompt events from Glean."
  );
}

function checkDefaultIcon(icon) {
  is(
    icon,
    "chrome://mozapps/skin/extensions/extensionGeneric.svg",
    "Popup has the default extension icon"
  );
}

add_task(() =>
  backgroundUpdateTest(
    `${BASE}/browser_webext_update1.xpi`,
    ID,
    checkDefaultIcon
  )
);
function checkNonDefaultIcon(icon) {
  // The icon should come from the extension, don't bother with the precise
  // path, just make sure we've got a jar url pointing to the right path
  // inside the jar.
  ok(icon.startsWith("jar:file://"), "Icon is a jar url");
  ok(icon.endsWith("/icon.png"), "Icon is icon.png inside a jar");
}

add_task(() =>
  backgroundUpdateTest(
    `${BASE}/browser_webext_update_icon1.xpi`,
    ID_ICON,
    checkNonDefaultIcon
  )
);
