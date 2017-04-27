const {AddonManagerPrivate} = Cu.import("resource://gre/modules/AddonManager.jsm", {});

const ID = "update2@tests.mozilla.org";
const ID_ICON = "update_icon2@tests.mozilla.org";
const ID_PERMS = "update_perms@tests.mozilla.org";
const ID_LEGACY = "legacy_update@tests.mozilla.org";

requestLongerTimeout(2);

function promiseViewLoaded(tab, viewid) {
  let win = tab.linkedBrowser.contentWindow;
  if (win.gViewController && !win.gViewController.isLoading &&
      win.gViewController.currentViewId == viewid) {
     return Promise.resolve();
  }

  return new Promise(resolve => {
    function listener() {
      if (win.gViewController.currentViewId != viewid) {
        return;
      }
      win.document.removeEventListener("ViewChanged", listener);
      resolve();
    }
    win.document.addEventListener("ViewChanged", listener);
  });
}

function getBadgeStatus() {
  let menuButton = document.getElementById("PanelUI-menu-button");
  return menuButton.getAttribute("badge-status");
}

// Set some prefs that apply to all the tests in this file
add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({set: [
    // We don't have pre-pinned certificates for the local mochitest server
    ["extensions.install.requireBuiltInCerts", false],
    ["extensions.update.requireBuiltInCerts", false],
  ]});

  // Navigate away from the initial page so that about:addons always
  // opens in a new tab during tests
  gBrowser.selectedBrowser.loadURI("about:robots");
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  registerCleanupFunction(function*() {
    // Return to about:blank when we're done
    gBrowser.selectedBrowser.loadURI("about:blank");
    yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  });
});

hookExtensionsTelemetry();

// Helper function to test background updates.
function* backgroundUpdateTest(url, id, checkIconFn) {
  yield SpecialPowers.pushPrefEnv({set: [
    // Turn on background updates
    ["extensions.update.enabled", true],

    // Point updates to the local mochitest server
    ["extensions.update.background.url", `${BASE}/browser_webext_update.json`],
  ]});

  // Install version 1.0 of the test extension
  let addon = yield promiseInstallAddon(url);

  ok(addon, "Addon was installed");
  is(getBadgeStatus(), "", "Should not start out with an addon alert badge");

  // Trigger an update check and wait for the update for this addon
  // to be downloaded.
  let updatePromise = promiseInstallEvent(addon, "onDownloadEnded");

  AddonManagerPrivate.backgroundUpdateCheck();
  yield updatePromise;

  is(getBadgeStatus(), "addon-alert", "Should have addon alert badge");

  // Find the menu entry for the update
  yield PanelUI.show();

  let addons = document.getElementById("PanelUI-footer-addons");
  is(addons.children.length, 1, "Have a menu entry for the update");

  // Click the menu item
  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:addons");
  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  addons.children[0].click();

  // about:addons should load and go to the list of extensions
  let tab = yield tabPromise;
  is(tab.linkedBrowser.currentURI.spec, "about:addons", "Browser is at about:addons");

  const VIEW = "addons://list/extension";
  yield promiseViewLoaded(tab, VIEW);
  let win = tab.linkedBrowser.contentWindow;
  ok(!win.gViewController.isLoading, "about:addons view is fully loaded");
  is(win.gViewController.currentViewId, VIEW, "about:addons is at extensions list");

  // Wait for the permission prompt, check the contents
  let panel = yield popupPromise;
  checkIconFn(panel.getAttribute("icon"));

  // The original extension has 1 promptable permission and the new one
  // has 2 (history and <all_urls>) plus 1 non-promptable permission (cookies).
  // So we should only see the 1 new promptable permission in the notification.
  let list = document.getElementById("addon-webext-perm-list");
  is(list.childElementCount, 1, "Permissions list contains 1 entry");

  // Cancel the update.
  panel.secondaryButton.click();

  addon = yield AddonManager.getAddonByID(id);
  is(addon.version, "1.0", "Should still be running the old version");

  yield BrowserTestUtils.removeTab(tab);

  // Alert badge and hamburger menu items should be gone
  is(getBadgeStatus(), "", "Addon alert badge should be gone");

  yield PanelUI.show();
  addons = document.getElementById("PanelUI-footer-addons");
  is(addons.children.length, 0, "Update menu entries should be gone");
  yield PanelUI.hide();

  // Re-check for an update
  updatePromise = promiseInstallEvent(addon, "onDownloadEnded");
  yield AddonManagerPrivate.backgroundUpdateCheck();
  yield updatePromise;

  is(getBadgeStatus(), "addon-alert", "Should have addon alert badge");

  // Find the menu entry for the update
  yield PanelUI.show();

  addons = document.getElementById("PanelUI-footer-addons");
  is(addons.children.length, 1, "Have a menu entry for the update");

  // Click the menu item
  tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:addons");
  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  addons.children[0].click();

  // Wait for about:addons to load
  tab = yield tabPromise;
  is(tab.linkedBrowser.currentURI.spec, "about:addons");

  yield promiseViewLoaded(tab, VIEW);
  win = tab.linkedBrowser.contentWindow;
  ok(!win.gViewController.isLoading, "about:addons view is fully loaded");
  is(win.gViewController.currentViewId, VIEW, "about:addons is at extensions list");

  // Wait for the permission prompt and accept it this time
  updatePromise = waitForUpdate(addon);
  panel = yield popupPromise;
  panel.button.click();

  addon = yield updatePromise;
  is(addon.version, "2.0", "Should have upgraded to the new version");

  yield BrowserTestUtils.removeTab(tab);

  is(getBadgeStatus(), "", "Addon alert badge should be gone");

  // Should have recorded 1 canceled followed by 1 accepted update.
  expectTelemetry(["updateRejected", "updateAccepted"]);

  addon.uninstall();
  yield SpecialPowers.popPrefEnv();
}

function checkDefaultIcon(icon) {
  is(icon, "chrome://mozapps/skin/extensions/extensionGeneric.svg",
     "Popup has the default extension icon");
}

add_task(() => backgroundUpdateTest(`${BASE}/browser_webext_update1.xpi`,
                                    ID, checkDefaultIcon));
function checkNonDefaultIcon(icon) {
  // The icon should come from the extension, don't bother with the precise
  // path, just make sure we've got a jar url pointing to the right path
  // inside the jar.
  ok(icon.startsWith("jar:file://"), "Icon is a jar url");
  ok(icon.endsWith("/icon.png"), "Icon is icon.png inside a jar");
}

add_task(() => backgroundUpdateTest(`${BASE}/browser_webext_update_icon1.xpi`,
                                    ID_ICON, checkNonDefaultIcon));
