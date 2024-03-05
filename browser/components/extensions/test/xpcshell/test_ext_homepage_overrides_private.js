/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
const { HomePage } = ChromeUtils.importESModule(
  "resource:///modules/HomePage.sys.mjs"
);
const { ExtensionPermissions } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs"
);

const { createAppInfo, promiseShutdownManager, promiseStartupManager } =
  AddonTestUtils;

const EXTENSION_ID = "test_overrides@tests.mozilla.org";
const HOMEPAGE_EXTENSION_CONTROLLED =
  "browser.startup.homepage_override.extensionControlled";
const HOMEPAGE_PRIVATE_ALLOWED =
  "browser.startup.homepage_override.privateAllowed";
const HOMEPAGE_URL_PREF = "browser.startup.homepage";
const HOMEPAGE_URI = "webext-homepage-1.html";

Services.prefs.setBoolPref("browser.privatebrowsing.autostart", true);

AddonTestUtils.init(this);
AddonTestUtils.usePrivilegedSignatures = false;
AddonTestUtils.overrideCertDB();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

function promisePrefChange(pref) {
  return new Promise(resolve => {
    Services.prefs.addObserver(pref, function observer() {
      Services.prefs.removeObserver(pref, observer);
      resolve(arguments);
    });
  });
}

let defaultHomepageURL;

function verifyPrefSettings(controlled, allowed) {
  equal(
    Services.prefs.getBoolPref(HOMEPAGE_EXTENSION_CONTROLLED, false),
    controlled,
    "homepage extension controlled"
  );
  equal(
    Services.prefs.getBoolPref(HOMEPAGE_PRIVATE_ALLOWED, false),
    allowed,
    "homepage private permission after permission change"
  );

  if (controlled && allowed) {
    ok(
      HomePage.get().endsWith(HOMEPAGE_URI),
      "Home page url is overridden by the extension"
    );
  } else {
    equal(HomePage.get(), defaultHomepageURL, "Home page url is default.");
  }
}

async function promiseUpdatePrivatePermission(allowed, extension) {
  info(`update private allowed permission`);
  await Promise.all([
    promisePrefChange(HOMEPAGE_PRIVATE_ALLOWED),
    ExtensionPermissions[allowed ? "add" : "remove"](
      extension.id,
      { permissions: ["internal:privateBrowsingAllowed"], origins: [] },
      extension
    ),
  ]);

  verifyPrefSettings(true, allowed);
}

add_task(async function test_overrides_private() {
  await promiseStartupManager();

  let extensionInfo = {
    useAddonManager: "permanent",
    manifest: {
      version: "1.0",
      browser_specific_settings: {
        gecko: {
          id: EXTENSION_ID,
        },
      },
      chrome_settings_overrides: {
        homepage: HOMEPAGE_URI,
      },
    },
  };
  let extension = ExtensionTestUtils.loadExtension(extensionInfo);

  defaultHomepageURL = HomePage.get();

  await extension.startup();

  verifyPrefSettings(true, false);

  equal(HomePage.get(), defaultHomepageURL, "Home page url is default.");

  info("add permission to extension");
  await promiseUpdatePrivatePermission(true, extension.extension);
  info("remove permission from extension");
  await promiseUpdatePrivatePermission(false, extension.extension);
  // set back to true to test upgrade removing extension control
  info("add permission back to prepare for upgrade test");
  await promiseUpdatePrivatePermission(true, extension.extension);

  extensionInfo.manifest = {
    version: "2.0",
    browser_specific_settings: {
      gecko: {
        id: EXTENSION_ID,
      },
    },
  };

  await Promise.all([
    promisePrefChange(HOMEPAGE_URL_PREF),
    extension.upgrade(extensionInfo),
  ]);

  verifyPrefSettings(false, false);

  await extension.unload();
  await promiseShutdownManager();
});
