const {AddonManagerPrivate} = Cu.import("resource://gre/modules/AddonManager.jsm", {});

const URL_BASE = "https://example.com/browser/browser/base/content/test/general";
const ID = "update@tests.mozilla.org";
const ID_ICON = "update_icon@tests.mozilla.org";

function promiseInstallAddon(url) {
  return AddonManager.getInstallForURL(url, null, "application/x-xpinstall")
                     .then(install => {
                       ok(install, "Created install");
                       return new Promise(resolve => {
                         install.addListener({
                           onInstallEnded(_install, addon) {
                             resolve(addon);
                           },
                         });
                         install.install();
                       });
                     });
}

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

function promisePopupNotificationShown(name) {
  return new Promise(resolve => {
    function popupshown() {
      let notification = PopupNotifications.getNotification(name);
      if (!notification) { return; }

      ok(notification, `${name} notification shown`);
      ok(PopupNotifications.isPanelOpen, "notification panel open");

      PopupNotifications.panel.removeEventListener("popupshown", popupshown);
      resolve(PopupNotifications.panel.firstChild);
    }

    PopupNotifications.panel.addEventListener("popupshown", popupshown);
  });
}

function getBadgeStatus() {
  let menuButton = document.getElementById("PanelUI-menu-button");
  return menuButton.getAttribute("badge-status");
}

function promiseInstallEvent(addon, event) {
  return new Promise(resolve => {
    let listener = {};
    listener[event] = (install, ...args) => {
      if (install.addon.id == addon.id) {
        AddonManager.removeInstallListener(listener);
        resolve(...args);
      }
    };
    AddonManager.addInstallListener(listener);
  });
}

// Set some prefs that apply to all the tests in this file
add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({set: [
    // We don't have pre-pinned certificates for the local mochitest server
    ["extensions.install.requireBuiltInCerts", false],
    ["extensions.update.requireBuiltInCerts", false],

    // XXX remove this when prompts are enabled by default
    ["extensions.webextPermissionPrompts", true],
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

// Helper function to test background updates.
function* backgroundUpdateTest(url, id, checkIconFn) {
  yield SpecialPowers.pushPrefEnv({set: [
    // Turn on background updates
    ["extensions.update.enabled", true],

    // Point updates to the local mochitest server
    ["extensions.update.background.url", `${URL_BASE}/browser_webext_update.json`],
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

  // Wait for the permission prompt, check the contents, then cancel the update
  let panel = yield popupPromise;
  checkIconFn(panel.getAttribute("icon"));
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
  updatePromise = promiseInstallEvent(addon, "onInstallEnded");
  panel = yield popupPromise;
  panel.button.click();

  addon = yield updatePromise;
  is(addon.version, "2.0", "Should have upgraded to the new version");

  yield BrowserTestUtils.removeTab(tab);

  is(getBadgeStatus(), "", "Addon alert badge should be gone");

  addon.uninstall();
  yield SpecialPowers.popPrefEnv();
}

function checkDefaultIcon(icon) {
  is(icon, "chrome://mozapps/skin/extensions/extensionGeneric.svg",
     "Popup has the default extension icon");
}

add_task(() => backgroundUpdateTest(`${URL_BASE}/browser_webext_update1.xpi`,
                                    ID, checkDefaultIcon));

function checkNonDefaultIcon(icon) {
  // The icon should come from the extension, don't bother with the precise
  // path, just make sure we've got a jar url pointing to the right path
  // inside the jar.
  ok(icon.startsWith("jar:file://"), "Icon is a jar url");
  ok(icon.endsWith("/icon.png"), "Icon is icon.png inside a jar");
}

add_task(() => backgroundUpdateTest(`${URL_BASE}/browser_webext_update_icon1.xpi`,
                                    ID_ICON, checkNonDefaultIcon));

// Test that an update that adds new non-promptable permissions is just
// applied without showing a notification dialog.
add_task(function*() {
  yield SpecialPowers.pushPrefEnv({set: [
    // Turn on background updates
    ["extensions.update.enabled", true],

    // Point updates to the local mochitest server
    ["extensions.update.background.url", `${URL_BASE}/browser_webext_update.json`],
  ]});

  // Install version 1.0 of the test extension
  let addon = yield promiseInstallAddon(`${URL_BASE}/browser_webext_update_perms1.xpi`);

  ok(addon, "Addon was installed");

  let sawPopup = false;
  PopupNotifications.panel.addEventListener("popupshown",
                                            () => sawPopup = true,
                                            {once: true});

  // Trigger an update check and wait for the update to be applied.
  let updatePromise = promiseInstallEvent(addon, "onInstallEnded");
  AddonManagerPrivate.backgroundUpdateCheck();
  yield updatePromise;

  // There should be no notifications about the update
  is(getBadgeStatus(), "", "Should not have addon alert badge");

  yield PanelUI.show();
  let addons = document.getElementById("PanelUI-footer-addons");
  is(addons.children.length, 0, "Have 0 updates in the PanelUI menu");
  yield PanelUI.hide();

  ok(!sawPopup, "Should not have seen permissions notification");

  addon = yield AddonManager.getAddonByID("update_perms@tests.mozilla.org");
  is(addon.version, "2.0", "Update should have applied");

  addon.uninstall();
  yield SpecialPowers.popPrefEnv();
});

// Helper function to test a specific scenario for interactive updates.
// `checkFn` is a callable that triggers a check for updates.
// `autoUpdate` specifies whether the test should be run with
// updates applied automatically or not.
function* interactiveUpdateTest(autoUpdate, checkFn) {
  yield SpecialPowers.pushPrefEnv({set: [
    ["extensions.update.autoUpdateDefault", autoUpdate],

    // Point updates to the local mochitest server
    ["extensions.update.url", `${URL_BASE}/browser_webext_update.json`],
  ]});

  // Trigger an update check, manually applying the update if we're testing
  // without auto-update.
  function* triggerUpdate(win, addon) {
    let manualUpdatePromise;
    if (!autoUpdate) {
      manualUpdatePromise = new Promise(resolve => {
        let listener = {
          onNewInstall() {
            AddonManager.removeInstallListener(listener);
            resolve();
          },
        };
        AddonManager.addInstallListener(listener);
      });
    }

    checkFn(win, addon);

    if (manualUpdatePromise) {
      yield manualUpdatePromise;

      let item = win.document.getElementById("addon-list")
                    .children.find(_item => _item.value == ID);
      EventUtils.synthesizeMouseAtCenter(item._updateBtn, {}, win);
    }
  }

  // Install version 1.0 of the test extension
  let addon = yield promiseInstallAddon(`${URL_BASE}/browser_webext_update1.xpi`);
  ok(addon, "Addon was installed");
  is(addon.version, "1.0", "Version 1 of the addon is installed");

  // Open add-ons manager and navigate to extensions list
  let loadPromise = new Promise(resolve => {
    let listener = (subject, topic) => {
      if (subject.location.href == "about:addons") {
        Services.obs.removeObserver(listener, topic);
        resolve(subject);
      }
    };
    Services.obs.addObserver(listener, "EM-loaded", false);
  });
  let tab = gBrowser.addTab("about:addons");
  gBrowser.selectedTab = tab;
  let win = yield loadPromise;

  const VIEW = "addons://list/extension";
  let viewPromise = promiseViewLoaded(tab, VIEW);
  win.loadView(VIEW);
  yield viewPromise;

  // Trigger an update check
  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  yield triggerUpdate(win, addon);
  let panel = yield popupPromise;

  // Click the cancel button, wait to see the cancel event
  let cancelPromise = promiseInstallEvent(addon, "onInstallCancelled");
  panel.secondaryButton.click();
  yield cancelPromise;

  addon = yield AddonManager.getAddonByID(ID);
  is(addon.version, "1.0", "Should still be running the old version");

  // Trigger a new update check
  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  yield triggerUpdate(win, addon);

  // This time, accept the upgrade
  let updatePromise = promiseInstallEvent(addon, "onInstallEnded");
  panel = yield popupPromise;
  panel.button.click();

  addon = yield updatePromise;
  is(addon.version, "2.0", "Should have upgraded");

  yield BrowserTestUtils.removeTab(tab);
  addon.uninstall();
  yield SpecialPowers.popPrefEnv();
}

// Invoke the "Check for Updates" menu item
function checkAll(win) {
  win.gViewController.doCommand("cmd_findAllUpdates");
}

// Test "Check for Updates" with both auto-update settings
add_task(() => interactiveUpdateTest(true, checkAll));
add_task(() => interactiveUpdateTest(false, checkAll));


// Invoke an invidual extension's "Find Updates" menu item
function checkOne(win, addon) {
  win.gViewController.doCommand("cmd_findItemUpdates", addon);
}

// Test "Find Updates" with both auto-update settings
add_task(() => interactiveUpdateTest(true, checkOne));
add_task(() => interactiveUpdateTest(false, checkOne));
