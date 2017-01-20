const {AddonManagerPrivate} = Cu.import("resource://gre/modules/AddonManager.jsm", {});

const URL_BASE = "https://example.com/browser/browser/base/content/test/general";
const ID = "update@tests.mozilla.org";

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

function promiseUpdateDownloaded(addon) {
  return new Promise(resolve => {
    let listener = {
      onDownloadEnded(install) {
        if (install.addon.id == addon.id) {
          AddonManager.removeInstallListener(listener);
          resolve();
        }
      },
    };
    AddonManager.addInstallListener(listener);
  });
}

function promiseUpgrade(addon) {
  return new Promise(resolve => {
    let listener = {
      onInstallEnded(install, newAddon) {
        if (newAddon.id == addon.id) {
          AddonManager.removeInstallListener(listener);
          resolve(newAddon);
        }
      },
    };
    AddonManager.addInstallListener(listener);
  });
}

add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [
    // Turn on background updates
    ["extensions.update.enabled", true],

    // Point updates to the local mochitest server
    ["extensions.update.background.url", `${URL_BASE}/browser_webext_update.json`],

    // We don't have pre-pinned certificates for the local mochitest server
    ["extensions.install.requireBuiltInCerts", false],
    ["extensions.update.requireBuiltInCerts", false],

    // XXX remove this when prompts are enabled by default
    ["extensions.webextPermissionPrompts", true],
  ]});

  // Install version 1.0 of the test extension
  let url1 = `${URL_BASE}/browser_webext_update1.xpi`;
  let install = yield AddonManager.getInstallForURL(url1, null, "application/x-xpinstall");
  ok(install, "Created install");

  let addon = yield new Promise(resolve => {
    install.addListener({
      onInstallEnded(_install, _addon) {
        resolve(_addon);
      },
    });
    install.install();
  });

  ok(addon, "Addon was installed");
  is(getBadgeStatus(), "", "Should not start out with an addon alert badge");

  // Trigger an update check and wait for the update for this addon
  // to be downloaded.
  let updatePromise = promiseUpdateDownloaded(addon);
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
  is(tab.linkedBrowser.currentURI.spec, "about:addons");

  const VIEW = "addons://list/extension";
  yield promiseViewLoaded(tab, VIEW);
  let win = tab.linkedBrowser.contentWindow;
  ok(!win.gViewController.isLoading, "about:addons view is fully loaded");
  is(win.gViewController.currentViewId, VIEW, "about:addons is at extensions list");

  // Wait for the permission prompt and cancel it
  let panel = yield popupPromise;
  panel.secondaryButton.click();

  addon = yield AddonManager.getAddonByID(ID);
  is(addon.version, "1.0", "Should still be running the old version");

  yield BrowserTestUtils.removeTab(tab);

  // Alert badge and hamburger menu items should be gone
  is(getBadgeStatus(), "", "Addon alert badge should be gone");

  yield PanelUI.show();
  addons = document.getElementById("PanelUI-footer-addons");
  is(addons.children.length, 0, "Update menu entries should be gone");
  yield PanelUI.hide();

  // Re-check for an update
  updatePromise = promiseUpdateDownloaded(addon);
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
  updatePromise = promiseUpgrade(addon);
  panel = yield popupPromise;
  panel.button.click();

  addon = yield updatePromise;
  is(addon.version, "2.0", "Should have upgraded to the new version");

  yield BrowserTestUtils.removeTab(tab);

  is(getBadgeStatus(), "", "Addon alert badge should be gone");
});
