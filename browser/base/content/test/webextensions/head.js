
const BASE = getRootDirectory(gTestPath)
  .replace("chrome://mochitests/content/", "https://example.com/");

ChromeUtils.import("resource:///modules/ExtensionsUI.jsm");
XPCOMUtils.defineLazyGetter(this, "Management", () => {
  // eslint-disable-next-line no-shadow
  const {Management} = ChromeUtils.import("resource://gre/modules/Extension.jsm", {});
  return Management;
});

ChromeUtils.import("resource://testing-common/CustomizableUITestUtils.jsm", this);
let gCUITestUtils = new CustomizableUITestUtils(window);

/**
 * Wait for the given PopupNotification to display
 *
 * @param {string} name
 *        The name of the notification to wait for.
 *
 * @returns {Promise}
 *          Resolves with the notification window.
 */
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

/**
 * Wait for a specific install event to fire for a given addon
 *
 * @param {AddonWrapper} addon
 *        The addon to watch for an event on
 * @param {string}
 *        The name of the event to watch for (e.g., onInstallEnded)
 *
 * @returns {Promise}
 *          Resolves when the event triggers with the first argument
 *          to the event handler as the resolution value.
 */
function promiseInstallEvent(addon, event) {
  return new Promise(resolve => {
    let listener = {};
    listener[event] = (install, arg) => {
      if (install.addon.id == addon.id) {
        AddonManager.removeInstallListener(listener);
        resolve(arg);
      }
    };
    AddonManager.addInstallListener(listener);
  });
}

/**
 * Install an (xpi packaged) extension
 *
 * @param {string} url
 *        URL of the .xpi file to install
 *
 * @returns {Promise}
 *          Resolves when the extension has been installed with the Addon
 *          object as the resolution value.
 */
async function promiseInstallAddon(url) {
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  install.install();

  let addon = await new Promise(resolve => {
    install.addListener({
      onInstallEnded(_install, _addon) {
        resolve(_addon);
      },
    });
  });

  if (addon.isWebExtension) {
    await new Promise(resolve => {
      function listener(event, extension) {
        if (extension.id == addon.id) {
          Management.off("ready", listener);
          resolve();
        }
      }
      Management.on("ready", listener);
    });
  }

  return addon;
}

/**
 * Wait for an update to the given webextension to complete.
 * (This does not actually perform an update, it just watches for
 * the events that occur as a result of an update.)
 *
 * @param {AddonWrapper} addon
 *        The addon to be updated.
 *
 * @returns {Promise}
 *          Resolves when the extension has ben updated.
 */
async function waitForUpdate(addon) {
  let installPromise = promiseInstallEvent(addon, "onInstallEnded");
  let readyPromise = new Promise(resolve => {
    function listener(event, extension) {
      if (extension.id == addon.id) {
        Management.off("ready", listener);
        resolve();
      }
    }
    Management.on("ready", listener);
  });

  let [newAddon, ] = await Promise.all([installPromise, readyPromise]);
  return newAddon;
}

function isDefaultIcon(icon) {
  // These are basically the same icon, but code within webextensions
  // generates references to the former and generic add-ons manager code
  // generates referces to the latter.
  return (icon == "chrome://browser/content/extension.svg" ||
          icon == "chrome://mozapps/skin/extensions/extensionGeneric.svg");
}

function is_hidden(element) {
  var style = element.ownerGlobal.getComputedStyle(element);
  if (style.display == "none")
    return true;
  if (style.visibility != "visible")
    return true;
  if (style.display == "-moz-popup")
    return ["hiding", "closed"].includes(element.state);

  // Hiding a parent element will hide all its children
  if (element.parentNode != element.ownerDocument)
    return is_hidden(element.parentNode);

  return false;
}

function is_visible(element) {
  var style = element.ownerGlobal.getComputedStyle(element);
  if (style.display == "none")
    return false;
  if (style.visibility != "visible")
    return false;
  if (style.display == "-moz-popup" && element.state != "open")
    return false;

  // Hiding a parent element will hide all its children
  if (element.parentNode != element.ownerDocument)
    return is_visible(element.parentNode);

  return true;
}

/**
 * Check the contents of an individual permission string.
 * This function is fairly specific to the use here and probably not
 * suitable for re-use elsewhere...
 *
 * @param {string} string
 *        The string value to check (i.e., pulled from the DOM)
 * @param {string} key
 *        The key in browser.properties for the localized string to
 *        compare with.
 * @param {string|null} param
 *        Optional string to substitute for %S in the localized string.
 * @param {string} msg
 *        The message to be emitted as part of the actual test.
 */
function checkPermissionString(string, key, param, msg) {
  let localizedString = param ?
                        gBrowserBundle.formatStringFromName(key, [param], 1) :
                        gBrowserBundle.GetStringFromName(key);

  // If this is a parameterized string and the parameter isn't given,
  // just do a simple comparison of the text before and after the %S
  if (localizedString.includes("%S")) {
    let i = localizedString.indexOf("%S");
    ok(string.startsWith(localizedString.slice(0, i)), msg);
    ok(string.endsWith(localizedString.slice(i + 2)), msg);
  } else {
    is(string, localizedString, msg);
  }
}

/**
 * Check the contents of a permission popup notification
 *
 * @param {Window} panel
 *        The popup window.
 * @param {string|regexp|function} checkIcon
 *        The icon expected to appear in the notification.  If this is a
 *        string, it must match the icon url exactly.  If it is a
 *        regular expression it is tested against the icon url, and if
 *        it is a function, it is called with the icon url and returns
 *        true if the url is correct.
 * @param {array} permissions
 *        The expected entries in the permissions list.  Each element
 *        in this array is itself a 2-element array with the string key
 *        for the item (e.g., "webextPerms.description.foo") and an
 *        optional formatting parameter.
 */
function checkNotification(panel, checkIcon, permissions) {
  let icon = panel.getAttribute("icon");
  let ul = document.getElementById("addon-webext-perm-list");
  let header = document.getElementById("addon-webext-perm-intro");

  if (checkIcon instanceof RegExp) {
    ok(checkIcon.test(icon), `Notification icon is correct ${JSON.stringify(icon)} ~= ${checkIcon}`);
  } else if (typeof checkIcon == "function") {
    ok(checkIcon(icon), "Notification icon is correct");
  } else {
    is(icon, checkIcon, "Notification icon is correct");
  }

  is(ul.childElementCount, permissions.length, `Permissions list has ${permissions.length} entries`);
  if (permissions.length == 0) {
    is(header.getAttribute("hidden"), "true", "Permissions header is hidden");
  } else {
    is(header.getAttribute("hidden"), "", "Permissions header is visible");
  }

  for (let i in permissions) {
    let [key, param] = permissions[i];
    checkPermissionString(ul.children[i].textContent, key, param,
                          `Permission number ${i + 1} is correct`);
  }
}

/**
 * Test that install-time permission prompts work for a given
 * installation method.
 *
 * @param {Function} installFn
 *        Callable that takes the name of an xpi file to install and
 *        starts to install it.  Should return a Promise that resolves
 *        when the install is finished or rejects if the install is canceled.
 * @param {string} telemetryBase
 *        If supplied, the base type for telemetry events that should be
 *        recorded for this install method.
 *
 * @returns {Promise}
 */
async function testInstallMethod(installFn, telemetryBase) {
  const PERMS_XPI = "browser_webext_permissions.xpi";
  const NO_PERMS_XPI = "browser_webext_nopermissions.xpi";
  const ID = "permissions@test.mozilla.org";

  await SpecialPowers.pushPrefEnv({set: [
    ["extensions.webapi.testing", true],
    ["extensions.install.requireBuiltInCerts", false],
  ]});

  if (telemetryBase !== undefined) {
    hookExtensionsTelemetry();
  }

  let testURI = makeURI("https://example.com/");
  Services.perms.add(testURI, "install", Services.perms.ALLOW_ACTION);
  registerCleanupFunction(() => Services.perms.remove(testURI, "install"));

  async function runOnce(filename, cancel) {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

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

    let panel = await promisePopupNotificationShown("addon-webext-permissions");
    if (filename == PERMS_XPI) {
      // The icon should come from the extension, don't bother with the precise
      // path, just make sure we've got a jar url pointing to the right path
      // inside the jar.
      checkNotification(panel, /^jar:file:\/\/.*\/icon\.png$/, [
        ["webextPerms.hostDescription.wildcard", "wildcard.domain"],
        ["webextPerms.hostDescription.oneSite", "singlehost.domain"],
        ["webextPerms.description.nativeMessaging"],
        // The below permissions are deliberately in this order as permissions
        // are sorted alphabetically by the permission string to match AMO.
        ["webextPerms.description.history"],
        ["webextPerms.description.tabs"],
      ]);
    } else if (filename == NO_PERMS_XPI) {
      checkNotification(panel, isDefaultIcon, []);
    }

    if (cancel) {
      panel.secondaryButton.click();
      try {
        await installMethodPromise;
      } catch (err) {}
    } else {
      // Look for post-install notification
      let postInstallPromise = promisePopupNotificationShown("addon-installed");
      panel.button.click();

      // Press OK on the post-install notification
      panel = await postInstallPromise;
      panel.button.click();

      await installMethodPromise;
    }

    let result = await installPromise;
    let addon = await AddonManager.getAddonByID(ID);
    if (cancel) {
      ok(!result, "Installation was cancelled");
      is(addon, null, "Extension is not installed");
    } else {
      ok(result, "Installation completed");
      isnot(addon, null, "Extension is installed");
      addon.uninstall();
    }

    BrowserTestUtils.removeTab(tab);
  }

  // A few different tests for each installation method:
  // 1. Start installation of an extension that requests no permissions,
  //    verify the notification contents, then cancel the install
  await runOnce(NO_PERMS_XPI, true);

  // 2. Same as #1 but with an extension that requests some permissions.
  await runOnce(PERMS_XPI, true);

  // 3. Repeat with the same extension from step 2 but this time,
  //    accept the permissions to install the extension.  (Then uninstall
  //    the extension to clean up.)
  await runOnce(PERMS_XPI, false);

  if (telemetryBase !== undefined) {
    // Should see 2 canceled installs followed by 1 successful install
    // for this method.
    expectTelemetry([`${telemetryBase}Rejected`, `${telemetryBase}Rejected`, `${telemetryBase}Accepted`]);
  }

  await SpecialPowers.popPrefEnv();
}

// Helper function to test a specific scenario for interactive updates.
// `checkFn` is a callable that triggers a check for updates.
// `autoUpdate` specifies whether the test should be run with
// updates applied automatically or not.
async function interactiveUpdateTest(autoUpdate, checkFn) {
  const ID = "update2@tests.mozilla.org";

  await SpecialPowers.pushPrefEnv({set: [
    // We don't have pre-pinned certificates for the local mochitest server
    ["extensions.install.requireBuiltInCerts", false],
    ["extensions.update.requireBuiltInCerts", false],

    ["extensions.update.autoUpdateDefault", autoUpdate],

    // Point updates to the local mochitest server
    ["extensions.update.url", `${BASE}/browser_webext_update.json`],
  ]});

  // Trigger an update check, manually applying the update if we're testing
  // without auto-update.
  async function triggerUpdate(win, addon) {
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

    let promise = checkFn(win, addon);

    if (manualUpdatePromise) {
      await manualUpdatePromise;

      let list = win.document.getElementById("addon-list");

      // Make sure we have XBL bindings
      list.clientHeight;

      let item = list.children.find(_item => _item.value == ID);
      EventUtils.synthesizeMouseAtCenter(item._updateBtn, {}, win);
    }

    return {promise};
  }

  // Navigate away from the starting page to force about:addons to load
  // in a new tab during the tests below.
  gBrowser.selectedBrowser.loadURI("about:robots");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  // Install version 1.0 of the test extension
  let addon = await promiseInstallAddon(`${BASE}/browser_webext_update1.xpi`);
  ok(addon, "Addon was installed");
  is(addon.version, "1.0", "Version 1 of the addon is installed");

  let win = await BrowserOpenAddonsMgr("addons://list/extension");

  // Trigger an update check
  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  let {promise: checkPromise} = await triggerUpdate(win, addon);
  let panel = await popupPromise;

  // Click the cancel button, wait to see the cancel event
  let cancelPromise = promiseInstallEvent(addon, "onInstallCancelled");
  panel.secondaryButton.click();
  await cancelPromise;

  addon = await AddonManager.getAddonByID(ID);
  is(addon.version, "1.0", "Should still be running the old version");

  // Make sure the update check is completely finished.
  await checkPromise;

  // Trigger a new update check
  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  checkPromise = (await triggerUpdate(win, addon)).promise;

  // This time, accept the upgrade
  let updatePromise = waitForUpdate(addon);
  panel = await popupPromise;
  panel.button.click();

  addon = await updatePromise;
  is(addon.version, "2.0", "Should have upgraded");

  await checkPromise;

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  addon.uninstall();
  await SpecialPowers.popPrefEnv();
}

// The tests in this directory install a bunch of extensions but they
// need to uninstall them before exiting, as a stray leftover extension
// after one test can foul up subsequent tests.
// So, add a task to run before any tests that grabs a list of all the
// add-ons that are pre-installed in the test environment and then checks
// the list of installed add-ons at the end of the test to make sure no
// new add-ons have been added.
// Individual tests can store a cleanup function in the testCleanup global
// to ensure it gets called before the final check is performed.
let testCleanup;
add_task(async function() {
  let addons = await AddonManager.getAllAddons();
  let existingAddons = new Set(addons.map(a => a.id));

  registerCleanupFunction(async function() {
    if (testCleanup) {
      await testCleanup();
      testCleanup = null;
    }

    for (let addon of await AddonManager.getAllAddons()) {
      if (!existingAddons.has(addon.id)) {
        ok(false, `Addon ${addon.id} was left installed at the end of the test`);
        addon.uninstall();
      }
    }
  });
});

let collectedTelemetry = [];
function hookExtensionsTelemetry() {
  let originalHistogram = ExtensionsUI.histogram;
  ExtensionsUI.histogram = {
    add(value) { collectedTelemetry.push(value); },
  };
  registerCleanupFunction(() => {
    is(collectedTelemetry.length, 0, "No unexamined telemetry after test is finished");
    ExtensionsUI.histogram = originalHistogram;
  });
}

function expectTelemetry(values) {
  Assert.deepEqual(values, collectedTelemetry);
  collectedTelemetry = [];
}

