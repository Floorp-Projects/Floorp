
const BASE = getRootDirectory(gTestPath)
  .replace("chrome://mochitests/content/", "https://example.com/");

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
    return ["hiding", "closed"].indexOf(element.state) != -1;

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
 * Test that install-time permission prompts work for a given
 * installation method.
 *
 * @param {Function} installFn
 *        Callable that takes the name of an xpi file to install and
 *        starts to install it.  Should return a Promise that resolves
 *        when the install is finished or rejects if the install is canceled.
 *
 * @returns {Promise}
 */
async function testInstallMethod(installFn) {
  const PERMS_XPI = "browser_webext_permissions.xpi";
  const NO_PERMS_XPI = "browser_webext_nopermissions.xpi";
  const ID = "permissions@test.mozilla.org";

  await SpecialPowers.pushPrefEnv({set: [
    ["extensions.webapi.testing", true],
    ["extensions.install.requireBuiltInCerts", false],

    // XXX remove this when prompts are enabled by default
    ["extensions.webextPermissionPrompts", true],
  ]});

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
      is(ul.childElementCount, 5, "Permissions list has 5 entries");

      checkPermissionString(ul.children[0].textContent,
                            "webextPerms.hostDescription.wildcard",
                            "wildcard.domain",
                            "First permission is domain permission");
      checkPermissionString(ul.children[1].textContent,
                            "webextPerms.hostDescription.oneSite",
                            "singlehost.domain",
                            "Second permission is single host permission");
      checkPermissionString(ul.children[2].textContent,
                            "webextPerms.description.nativeMessaging", null,
                            "Third permission is nativeMessaging");
      checkPermissionString(ul.children[3].textContent,
                            "webextPerms.description.tabs", null,
                            "Fourth permission is tabs");
      checkPermissionString(ul.children[4].textContent,
                            "webextPerms.description.history", null,
                            "Fifth permission is history");
    } else if (filename == NO_PERMS_XPI) {
      // This extension has no icon, it should have the default
      ok(isDefaultIcon(icon), "Icon is the default extension icon");

      is(header.getAttribute("hidden"), "true", "Permission list header is hidden");
      is(ul.childElementCount, 0, "Permissions list has 0 entries");
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

    await BrowserTestUtils.removeTab(tab);
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
