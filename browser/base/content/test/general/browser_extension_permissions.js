"use strict";

const BASE = getRootDirectory(gTestPath)
  .replace("chrome://mochitests/content/", "https://example.com/");

const INSTALL_PAGE = `${BASE}/file_install_extensions.html`;
const PERMS_XPI = "browser_webext_permissions.xpi";
const NO_PERMS_XPI = "browser_webext_nopermissions.xpi";
const ID = "permissions@test.mozilla.org";

Services.perms.add(makeURI("https://example.com/"), "install",
                   Services.perms.ALLOW_ACTION);

registerCleanupFunction(async function() {
  let addon = await AddonManager.getAddonByID(ID);
  if (addon) {
    ok(false, `Addon ${ID} was still installed at the end of the test`);
    addon.uninstall();
  }
});

function isDefaultIcon(icon) {
  // These are basically the same icon, but code within webextensions
  // generates references to the former and generic add-ons manager code
  // generates referces to the latter.
  return (icon == "chrome://browser/content/extension.svg" ||
          icon == "chrome://mozapps/skin/extensions/extensionGeneric.svg");
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

function checkNotification(panel, filename) {
  let icon = panel.getAttribute("icon");

  let ul = document.getElementById("addon-webext-perm-list");
  let header = document.getElementById("addon-webext-perm-intro");

  if (filename == PERMS_XPI) {
    // The icon should come from the extension, don't bother with the precise
    // path, just make sure we've got a jar url pointing to the right path
    // inside the jar.
    ok(icon.startsWith("jar:file://"), "Icon is a jar url");
    ok(icon.endsWith("/icon.png"), "Icon is icon.png inside a jar");

    is(header.getAttribute("hidden"), "", "Permission list header is visible");
    is(ul.childElementCount, 4, "Permissions list has 4 entries");
    // Real checking of the contents here is deferred until bug 1316996 lands
  } else if (filename == NO_PERMS_XPI) {
    // This extension has no icon, it should have the default
    ok(isDefaultIcon(icon), "Icon is the default extension icon");

    is(header.getAttribute("hidden"), "true", "Permission list header is hidden");
    is(ul.childElementCount, 0, "Permissions list has 0 entries");
  }
}

// Navigate the current tab to the given url and return a Promise
// that resolves when the page is loaded.
function load(url) {
  gBrowser.selectedBrowser.loadURI(INSTALL_PAGE);
  return BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
}

const INSTALL_FUNCTIONS = [
  async function installMozAM(filename) {
    await load(INSTALL_PAGE);

    await ContentTask.spawn(gBrowser.selectedBrowser, `${BASE}/${filename}`, function*(url) {
      yield content.wrappedJSObject.installMozAM(url);
    });
  },

  async function installTrigger(filename) {
    await load(INSTALL_PAGE);

    ContentTask.spawn(gBrowser.selectedBrowser, `${BASE}/${filename}`, function*(url) {
      content.wrappedJSObject.installTrigger(url);
    });
  },

  async function installFile(filename) {
    const ChromeRegistry = Cc["@mozilla.org/chrome/chrome-registry;1"]
                                       .getService(Ci.nsIChromeRegistry);
    let chromeUrl = Services.io.newURI(gTestPath);
    let fileUrl = ChromeRegistry.convertChromeURL(chromeUrl);
    let file = fileUrl.QueryInterface(Ci.nsIFileURL).file;
    file.leafName = filename;

    let MockFilePicker = SpecialPowers.MockFilePicker;
    MockFilePicker.init(window);
    MockFilePicker.returnFiles = [file];

    await BrowserOpenAddonsMgr("addons://list/extension");
    let contentWin = gBrowser.selectedTab.linkedBrowser.contentWindow;

    // Do the install...
    contentWin.gViewController.doCommand("cmd_installFromFile");
    MockFilePicker.cleanup();
  },

  async function installSearch(filename) {
    await SpecialPowers.pushPrefEnv({set: [
      ["extensions.getAddons.maxResults", 10],
      ["extensions.getAddons.search.url", `${BASE}/browser_webext_search.xml`],
    ]});

    let win = await BrowserOpenAddonsMgr("addons://list/extension");

    let searchResultsPromise = new Promise(resolve => {
      win.document.addEventListener("ViewChanged", resolve, {once: true});
    });
    let search = win.document.getElementById("header-search");
    search.focus();
    search.value = "search text";
    EventUtils.synthesizeKey("VK_RETURN", {}, win);

    await searchResultsPromise;
    ok(win.gViewController.currentViewId.startsWith("addons://search"),
       "about:addons is displaying search results");

    let list = win.document.getElementById("search-list");
    let item = null;
    for (let child of list.childNodes) {
      if (child.nodeName == "richlistitem" &&
          child.mAddon.install.sourceURI.path.endsWith(filename)) {
            item = child;
            break;
      }
    }
    ok(item, `Found ${filename} in search results`);

    // abracadabara XBL
    item.clientTop;

    let install = win.document.getAnonymousElementByAttribute(item, "anonid", "install-status");
    let button = win.document.getAnonymousElementByAttribute(install, "anonid", "install-remote-btn");
    EventUtils.synthesizeMouseAtCenter(button, {}, win);
  },
];

add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [
    ["extensions.webapi.testing", true],
    ["extensions.install.requireBuiltInCerts", false],

    // XXX remove this when prompts are enabled by default
    ["extensions.webextPermissionPrompts", true],
  ]});

  function* runOnce(installFn, filename, cancel) {
    let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser);

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

    let installMethodPromise = installFn(filename);

    let panel = yield promisePopupNotificationShown("addon-webext-permissions");
    checkNotification(panel, filename);

    if (cancel) {
      panel.secondaryButton.click();
      try {
        yield installMethodPromise;
      } catch (err) {}
    } else {
      // Look for post-install notification
      let postInstallPromise = promisePopupNotificationShown("addon-installed");
      panel.button.click();

      // Press OK on the post-install notification
      panel = yield postInstallPromise;
      panel.button.click();

      yield installMethodPromise;
    }

    let result = yield installPromise;
    let addon = yield AddonManager.getAddonByID(ID);
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
