ChromeUtils.defineESModuleGetters(this, {
  AddonTestUtils: "resource://testing-common/AddonTestUtils.sys.mjs",
});

const BASE = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);

ChromeUtils.defineLazyGetter(this, "Management", () => {
  // eslint-disable-next-line no-shadow
  const { Management } = ChromeUtils.importESModule(
    "resource://gre/modules/Extension.sys.mjs"
  );
  return Management;
});

let { CustomizableUITestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CustomizableUITestUtils.sys.mjs"
);
let gCUITestUtils = new CustomizableUITestUtils(window);

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

let extL10n = null;
/**
 * @param {string} id
 * @param {object} [args]
 * @returns {string}
 */
function formatExtValue(id, args) {
  if (!extL10n) {
    extL10n = new Localization(
      [
        "toolkit/global/extensions.ftl",
        "toolkit/global/extensionPermissions.ftl",
        "branding/brand.ftl",
      ],
      true
    );
  }
  return extL10n.formatValueSync(id, args);
}

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
      if (!notification) {
        return;
      }

      ok(notification, `${name} notification shown`);
      ok(PopupNotifications.isPanelOpen, "notification panel open");

      PopupNotifications.panel.removeEventListener("popupshown", popupshown);
      resolve(PopupNotifications.panel.firstElementChild);
    }

    PopupNotifications.panel.addEventListener("popupshown", popupshown);
  });
}

function promiseAppMenuNotificationShown(id) {
  const { AppMenuNotifications } = ChromeUtils.importESModule(
    "resource://gre/modules/AppMenuNotifications.sys.mjs"
  );
  return new Promise(resolve => {
    function popupshown() {
      let notification = AppMenuNotifications.activeNotification;
      if (!notification) {
        return;
      }

      is(notification.id, id, `${id} notification shown`);
      ok(PanelUI.isNotificationPanelOpen, "notification panel open");

      PanelUI.notificationPanel.removeEventListener("popupshown", popupshown);

      let popupnotificationID = PanelUI._getPopupId(notification);
      let popupnotification = document.getElementById(popupnotificationID);

      resolve(popupnotification);
    }
    PanelUI.notificationPanel.addEventListener("popupshown", popupshown);
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
 * @param {Object?} installTelemetryInfo
 *        an optional object that contains additional details used by the telemetry events.
 *
 * @returns {Promise}
 *          Resolves when the extension has been installed with the Addon
 *          object as the resolution value.
 */
async function promiseInstallAddon(url, telemetryInfo) {
  let install = await AddonManager.getInstallForURL(url, { telemetryInfo });
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

  let [newAddon] = await Promise.all([installPromise, readyPromise]);
  return newAddon;
}

function waitAboutAddonsViewLoaded(doc) {
  return BrowserTestUtils.waitForEvent(doc, "view-loaded");
}

/**
 * Trigger an action from the page options menu.
 */
function triggerPageOptionsAction(win, action) {
  win.document.querySelector(`#page-options [action="${action}"]`).click();
}

function isDefaultIcon(icon) {
  return icon == "chrome://mozapps/skin/extensions/extensionGeneric.svg";
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
 *        for the item (e.g., "webext-perms-description-foo") and an
 *        optional formatting parameter.
 * @param {boolean} sideloaded
 *        Whether the notification is for a sideloaded extenion.
 */
function checkNotification(panel, checkIcon, permissions, sideloaded) {
  let icon = panel.getAttribute("icon");
  let ul = document.getElementById("addon-webext-perm-list");
  let singleDataEl = document.getElementById("addon-webext-perm-single-entry");
  let learnMoreLink = document.getElementById("addon-webext-perm-info");

  if (checkIcon instanceof RegExp) {
    ok(
      checkIcon.test(icon),
      `Notification icon is correct ${JSON.stringify(icon)} ~= ${checkIcon}`
    );
  } else if (typeof checkIcon == "function") {
    ok(checkIcon(icon), "Notification icon is correct");
  } else {
    is(icon, checkIcon, "Notification icon is correct");
  }

  let description = panel.querySelector(
    ".popup-notification-description"
  ).textContent;
  let descL10nId = "webext-perms-header";
  if (permissions.length) {
    descL10nId = "webext-perms-header-with-perms";
  }
  if (sideloaded) {
    descL10nId = "webext-perms-sideload-header";
  }
  const exp = formatExtValue(descL10nId, { extension: "<>" }).split("<>");
  ok(description.startsWith(exp.at(0)), "Description is the expected one");
  ok(description.endsWith(exp.at(-1)), "Description is the expected one");

  is(
    learnMoreLink.hidden,
    !permissions.length,
    "Permissions learn more is hidden if there are no permissions"
  );

  if (!permissions.length) {
    ok(ul.hidden, "Permissions list is hidden");
    ok(singleDataEl.hidden, "Single permission data entry is hidden");
    ok(
      !(ul.childElementCount || singleDataEl.textContent),
      "Permission list and single permission element have no entries"
    );
  } else if (permissions.length === 1) {
    ok(ul.hidden, "Permissions list is hidden");
    ok(!ul.childElementCount, "Permission list has no entries");
    ok(singleDataEl.textContent, "Single permission data label has been set");
  } else {
    ok(singleDataEl.hidden, "Single permission data entry is hidden");
    ok(
      !singleDataEl.textContent,
      "Single permission data label has not been set"
    );
    for (let i in permissions) {
      let [key, param] = permissions[i];
      const expected = formatExtValue(key, param);
      is(
        ul.children[i].textContent,
        expected,
        `Permission number ${i + 1} is correct`
      );
    }
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

  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.webapi.testing", true],
      ["extensions.install.requireBuiltInCerts", false],
    ],
  });

  let testURI = makeURI("https://example.com/");
  PermissionTestUtils.add(testURI, "install", Services.perms.ALLOW_ACTION);
  registerCleanupFunction(() => PermissionTestUtils.remove(testURI, "install"));

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
        [
          "webext-perms-host-description-wildcard",
          { domain: "wildcard.domain" },
        ],
        [
          "webext-perms-host-description-one-site",
          { domain: "singlehost.domain" },
        ],
        ["webext-perms-description-nativeMessaging"],
        // The below permissions are deliberately in this order as permissions
        // are sorted alphabetically by the permission string to match AMO.
        ["webext-perms-description-history"],
        ["webext-perms-description-tabs"],
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
      let postInstallPromise =
        promiseAppMenuNotificationShown("addon-installed");
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
      await addon.uninstall();
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

  await SpecialPowers.popPrefEnv();
}

// Helper function to test a specific scenario for interactive updates.
// `checkFn` is a callable that triggers a check for updates.
// `autoUpdate` specifies whether the test should be run with
// updates applied automatically or not.
async function interactiveUpdateTest(autoUpdate, checkFn) {
  AddonTestUtils.initMochitest(this);
  Services.fog.testResetFOG();

  const ID = "update2@tests.mozilla.org";
  const FAKE_INSTALL_SOURCE = "fake-install-source";

  await SpecialPowers.pushPrefEnv({
    set: [
      // We don't have pre-pinned certificates for the local mochitest server
      ["extensions.install.requireBuiltInCerts", false],
      ["extensions.update.requireBuiltInCerts", false],

      ["extensions.update.autoUpdateDefault", autoUpdate],

      // Point updates to the local mochitest server
      ["extensions.update.url", `${BASE}/browser_webext_update.json`],
    ],
  });

  AddonTestUtils.hookAMTelemetryEvents();

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

      let doc = win.document;
      if (win.gViewController.currentViewId !== "addons://updates/available") {
        let showUpdatesBtn = doc.querySelector("addon-updates-message").button;
        await TestUtils.waitForCondition(() => {
          return !showUpdatesBtn.hidden;
        }, "Wait for show updates button");
        let viewChanged = waitAboutAddonsViewLoaded(doc);
        showUpdatesBtn.click();
        await viewChanged;
      }
      let card = await TestUtils.waitForCondition(() => {
        return doc.querySelector(`addon-card[addon-id="${ID}"]`);
      }, `Wait addon card for "${ID}"`);
      let updateBtn = card.querySelector('panel-item[action="install-update"]');
      ok(updateBtn, `Found update button for "${ID}"`);
      updateBtn.click();
    }

    return { promise };
  }

  // Navigate away from the starting page to force about:addons to load
  // in a new tab during the tests below.
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    "about:mozilla"
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  // Install version 1.0 of the test extension
  let addon = await promiseInstallAddon(`${BASE}/browser_webext_update1.xpi`, {
    source: FAKE_INSTALL_SOURCE,
  });
  ok(addon, "Addon was installed");
  is(addon.version, "1.0", "Version 1 of the addon is installed");

  let win = await BrowserOpenAddonsMgr("addons://list/extension");

  await waitAboutAddonsViewLoaded(win.document);

  // Trigger an update check
  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  let { promise: checkPromise } = await triggerUpdate(win, addon);
  let panel = await popupPromise;

  // Click the cancel button, wait to see the cancel event
  let cancelPromise = promiseInstallEvent(addon, "onInstallCancelled");
  panel.secondaryButton.click();
  const cancelledByUser = await cancelPromise;
  is(cancelledByUser, true, "Install cancelled by user");

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
  await addon.uninstall();
  await SpecialPowers.popPrefEnv();

  const collectedUpdateEvents = AddonTestUtils.getAMTelemetryEvents().filter(
    evt => {
      return evt.method === "update";
    }
  );

  const expectedSteps = [
    // First update is cancelled on the permission prompt.
    "started",
    "download_started",
    "download_completed",
    "permissions_prompt",
    "cancelled",
    // Second update is expected to be completed.
    "started",
    "download_started",
    "download_completed",
    "permissions_prompt",
    "completed",
  ];

  Assert.deepEqual(
    expectedSteps,
    collectedUpdateEvents.map(evt => evt.extra.step),
    "Got the expected sequence on update telemetry events"
  );

  let gleanEvents = AddonTestUtils.getAMGleanEvents("update");
  Services.fog.testResetFOG();

  Assert.deepEqual(
    expectedSteps,
    gleanEvents.map(e => e.step),
    "Got the expected sequence on update Glean events."
  );

  ok(
    collectedUpdateEvents.every(evt => evt.extra.addon_id === ID),
    "Every update telemetry event should have the expected addon_id extra var"
  );

  ok(
    collectedUpdateEvents.every(
      evt => evt.extra.source === FAKE_INSTALL_SOURCE
    ),
    "Every update telemetry event should have the expected source extra var"
  );

  ok(
    collectedUpdateEvents.every(evt => evt.extra.updated_from === "user"),
    "Every update telemetry event should have the update_from extra var 'user'"
  );

  for (let e of gleanEvents) {
    is(e.addon_id, ID, "Glean event has the expected addon_id.");
    is(e.source, FAKE_INSTALL_SOURCE, "Glean event has the expected source.");
    is(e.updated_from, "user", "Glean event has the expected updated_from.");

    if (e.step === "permissions_prompt") {
      Assert.greater(parseInt(e.num_strings), 0, "Expected num_strings.");
    }
    if (e.step === "download_completed") {
      Assert.greater(parseInt(e.download_time), 0, "Valid download_time.");
    }
  }

  let hasPermissionsExtras = collectedUpdateEvents
    .filter(evt => {
      return evt.extra.step === "permissions_prompt";
    })
    .every(evt => {
      return Number.isInteger(parseInt(evt.extra.num_strings, 10));
    });

  ok(
    hasPermissionsExtras,
    "Every 'permissions_prompt' update telemetry event should have the permissions extra vars"
  );

  let hasDownloadTimeExtras = collectedUpdateEvents
    .filter(evt => {
      return evt.extra.step === "download_completed";
    })
    .every(evt => {
      const download_time = parseInt(evt.extra.download_time, 10);
      return !isNaN(download_time) && download_time > 0;
    });

  ok(
    hasDownloadTimeExtras,
    "Every 'download_completed' update telemetry event should have a download_time extra vars"
  );
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
add_setup(async function head_setup() {
  let addons = await AddonManager.getAllAddons();
  let existingAddons = new Set(addons.map(a => a.id));

  registerCleanupFunction(async function () {
    if (testCleanup) {
      await testCleanup();
      testCleanup = null;
    }

    for (let addon of await AddonManager.getAllAddons()) {
      // Builtin search extensions may have been installed by SearchService
      // during the test run, ignore those.
      if (
        !existingAddons.has(addon.id) &&
        !(addon.isBuiltin && addon.id.endsWith("@search.mozilla.org"))
      ) {
        ok(
          false,
          `Addon ${addon.id} was left installed at the end of the test`
        );
        await addon.uninstall();
      }
    }
  });
});
