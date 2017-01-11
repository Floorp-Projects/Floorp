"use strict";

const BASE = getRootDirectory(gTestPath)
  .replace("chrome://mochitests/content/", "https://example.com/");

const PAGE = `${BASE}/file_install_extensions.html`;
const PERMS_XPI = `${BASE}/browser_webext_permissions.xpi`;
const NO_PERMS_XPI = `${BASE}/browser_webext_nopermissions.xpi`;
const ID = "permissions@test.mozilla.org";

const DEFAULT_EXTENSION_ICON = "chrome://browser/content/extension.svg";

Services.perms.add(makeURI("https://example.com/"), "install",
                   Services.perms.ALLOW_ACTION);

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

function promiseGetAddonByID(id) {
  return new Promise(resolve => {
    AddonManager.getAddonByID(id, resolve);
  });
}

function checkNotification(panel, url) {
  let icon = panel.getAttribute("icon");

  let uls = panel.firstChild.getElementsByTagNameNS("http://www.w3.org/1999/xhtml", "ul");
  is(uls.length, 1, "Found the permissions list");
  let ul = uls[0];

  let headers = panel.firstChild.getElementsByClassName("addon-webext-perm-text");
  is(headers.length, 1, "Found the header");
  let header = headers[0];

  if (url == PERMS_XPI) {
    // The icon should come from the extension, don't bother with the precise
    // path, just make sure we've got a jar url pointing to the right path
    // inside the jar.
    ok(icon.startsWith("jar:file://"), "Icon is a jar url");
    ok(icon.endsWith("/icon.png"), "Icon is icon.png inside a jar");

    is(header.getAttribute("hidden"), "", "Permission list header is visible");
    is(ul.childElementCount, 4, "Permissions list has 4 entries");
    // Real checking of the contents here is deferred until bug 1316996 lands
  } else if (url == NO_PERMS_XPI) {
    // This extension has no icon, it should have the default
    is(icon, DEFAULT_EXTENSION_ICON, "Icon is the default extension icon");

    is(header.getAttribute("hidden"), "true", "Permission list header is hidden");
    is(ul.childElementCount, 0, "Permissions list has 0 entries");
  }
}

const INSTALL_FUNCTIONS = [
  function installMozAM(url) {
    ContentTask.spawn(gBrowser.selectedBrowser, url, function*(cUrl) {
      content.wrappedJSObject.installMozAM(cUrl);
    });
  },

  function installTrigger(url) {
    ContentTask.spawn(gBrowser.selectedBrowser, url, function*(cUrl) {
      content.wrappedJSObject.installTrigger(cUrl);
    });
  },
];

add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [
    ["extensions.webapi.testing", true],
    ["extensions.install.requireBuiltInCerts", false],

    // XXX remove this when prompts are enabled by default
    ["extensions.webextPermissionPrompts", true],
  ]});

  function* runOnce(installFn, url, cancel) {
    let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

    let installPromise = new Promise(resolve => {
      let listener = {
        onDownloadCancelled() {
          AddonManager.removeInstallListener(listener);
          resolve(false);
        },

        onDownloadFailed() {
          AddonManager.removeInstallListener(listener);
          resolve(false);
        },

        onInstallCancelled() {
          AddonManager.removeInstallListener(listener);
          resolve(false);
        },

        onInstallEnded() {
          AddonManager.removeInstallListener(listener);
          resolve(true);
        },

        onInstallFailed() {
          AddonManager.removeInstallListener(listener);
          resolve(false);
        },
      };
      AddonManager.addInstallListener(listener);
    });

    installFn(url);

    let panel = yield promisePopupNotificationShown("addon-webext-permissions");
    checkNotification(panel, url);

    if (cancel) {
      panel.secondaryButton.click();
    } else {
      panel.button.click();
    }

    let result = yield installPromise;
    let addon = yield promiseGetAddonByID(ID);
    if (cancel) {
      ok(!result, "Installation was cancelled");
      is(addon, null, "Extension is not installed");
    } else {
      ok(result, "Installation completed");
      isnot(addon, null, "Extension is installed");
      addon.uninstall();
    }

    yield BrowserTestUtils.removeTab(tab);
  }

  for (let installFn of INSTALL_FUNCTIONS) {
    yield runOnce(installFn, NO_PERMS_XPI, true);
    yield runOnce(installFn, PERMS_XPI, true);
    yield runOnce(installFn, PERMS_XPI, false);
  }
});
