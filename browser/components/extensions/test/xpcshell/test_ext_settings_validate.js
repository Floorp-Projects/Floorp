/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

const { HomePage } = ChromeUtils.import("resource:///modules/HomePage.jsm");
const { ExtensionParent } = ChromeUtils.import(
  "resource://gre/modules/ExtensionParent.jsm"
);
const { apiManager } = ExtensionParent;

XPCOMUtils.defineLazyServiceGetter(
  this,
  "aboutNewTabService",
  "@mozilla.org/browser/aboutnewtab-service;1",
  "nsIAboutNewTabService"
);

AddonTestUtils.init(this);

// Allow for unsigned addons.
AddonTestUtils.overrideCertDB();

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

add_task(async function test_settings_modules_not_loaded() {
  await apiManager.lazyInit();
  // Test that no settings modules are loaded.
  let modules = Array.from(apiManager.settingsModules);
  ok(modules.length, "we have settings modules");
  for (let name of modules) {
    ok(!apiManager.getModule(name).loaded, `${name} is not loaded`);
  }
});

add_task(async function test_settings_validated() {
  let defaultNewTab = aboutNewTabService.newTabURL;
  equal(defaultNewTab, "about:newtab", "Newtab url is default.");
  let defaultHomepageURL = HomePage.get();
  equal(defaultHomepageURL, "about:home", "Home page url is default.");

  let xpi = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "1.0",
      applications: { gecko: { id: "test@mochi" } },
      chrome_url_overrides: {
        newtab: "/newtab",
      },
      chrome_settings_overrides: {
        homepage: "https://example.com/",
      },
    },
  });
  let extension = ExtensionTestUtils.expectExtension("test@mochi");
  let file = await AddonTestUtils.manuallyInstall(xpi);
  await AddonTestUtils.promiseStartupManager();
  await extension.awaitStartup();

  equal(
    HomePage.get(),
    "https://example.com/",
    "Home page url is extension controlled."
  );
  ok(
    aboutNewTabService.newTabURL.endsWith("/newtab"),
    "newTabURL is extension controlled."
  );

  await AddonTestUtils.promiseShutdownManager();
  // After shutdown, delete the xpi file.
  Services.obs.notifyObservers(xpi, "flush-cache-entry");
  try {
    file.remove(true);
  } catch (e) {
    ok(false, e);
  }
  await AddonTestUtils.cleanupTempXPIs();

  // Restart everything, the UninstallObserver should handle updating state.
  let prefChanged = TestUtils.waitForPrefChange("browser.startup.homepage");
  await AddonTestUtils.promiseStartupManager();
  await prefChanged;

  equal(HomePage.get(), defaultHomepageURL, "Home page url is default.");
  equal(
    aboutNewTabService.newTabURL,
    defaultNewTab,
    "newTabURL is reset to default."
  );
  await AddonTestUtils.promiseShutdownManager();
});

add_task(async function test_settings_validated_safemode_uninstall() {
  let defaultNewTab = aboutNewTabService.newTabURL;
  equal(defaultNewTab, "about:newtab", "Newtab url is default.");
  let defaultHomepageURL = HomePage.get();
  equal(defaultHomepageURL, "about:home", "Home page url is default.");

  let xpi = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "1.0",
      applications: { gecko: { id: "test@mochi" } },
      chrome_url_overrides: {
        newtab: "/newtab",
      },
      chrome_settings_overrides: {
        homepage: "https://example.com/",
      },
    },
  });
  let extension = ExtensionTestUtils.expectExtension("test@mochi");
  await AddonTestUtils.manuallyInstall(xpi);
  await AddonTestUtils.promiseStartupManager();
  await extension.awaitStartup();

  equal(
    HomePage.get(),
    "https://example.com/",
    "Home page url is extension controlled."
  );
  ok(
    aboutNewTabService.newTabURL.endsWith("/newtab"),
    "newTabURL is extension controlled."
  );

  await AddonTestUtils.promiseShutdownManager();
  AddonTestUtils.appInfo.inSafeMode = true;
  await AddonTestUtils.promiseStartupManager();
  let addon = await AddonManager.getAddonByID("test@mochi");

  // Restart everything, the UninstallObserver should handle updating state.
  let prefChanged = TestUtils.waitForPrefChange("browser.startup.homepage");
  await addon.uninstall();
  await prefChanged;

  equal(HomePage.get(), defaultHomepageURL, "Home page url is default.");
  equal(
    aboutNewTabService.newTabURL,
    defaultNewTab,
    "newTabURL is reset to default."
  );

  await AddonTestUtils.promiseShutdownManager();
  AddonTestUtils.appInfo.inSafeMode = false;
});

// There are more settings modules than used in this test file, they should have been
// loaded during the test extensions uninstall.  Ensure that all settings modules have
// been loaded.
add_task(async function test_settings_modules_loaded() {
  // Test that all settings modules are loaded.
  let modules = Array.from(apiManager.settingsModules);
  ok(modules.length, "we have settings modules");
  for (let name of modules) {
    ok(apiManager.getModule(name).loaded, `${name} was loaded`);
  }
});
