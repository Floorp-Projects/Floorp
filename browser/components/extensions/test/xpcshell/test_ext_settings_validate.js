/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs"
);

const { AboutNewTab } = ChromeUtils.importESModule(
  "resource:///modules/AboutNewTab.sys.mjs"
);

// Lazy load to avoid having Services.appinfo cached first.
ChromeUtils.defineESModuleGetters(this, {
  ExtensionParent: "resource://gre/modules/ExtensionParent.sys.mjs",
});

const { HomePage } = ChromeUtils.importESModule(
  "resource:///modules/HomePage.sys.mjs"
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
  await ExtensionParent.apiManager.lazyInit();
  // Test that no settings modules are loaded.
  let modules = Array.from(ExtensionParent.apiManager.settingsModules);
  ok(modules.length, "we have settings modules");
  for (let name of modules) {
    ok(
      !ExtensionParent.apiManager.getModule(name).loaded,
      `${name} is not loaded`
    );
  }
});

add_task(async function test_settings_validated() {
  let defaultNewTab = AboutNewTab.newTabURL;
  equal(defaultNewTab, "about:newtab", "Newtab url is default.");
  let defaultHomepageURL = HomePage.get();
  equal(defaultHomepageURL, "about:home", "Home page url is default.");

  let xpi = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "1.0",
      browser_specific_settings: { gecko: { id: "test@mochi" } },
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
    AboutNewTab.newTabURL.endsWith("/newtab"),
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

  // Restart everything, the ExtensionAddonObserver should handle updating state.
  let prefChanged = TestUtils.waitForPrefChange("browser.startup.homepage");
  await AddonTestUtils.promiseStartupManager();
  await prefChanged;

  equal(HomePage.get(), defaultHomepageURL, "Home page url is default.");
  equal(AboutNewTab.newTabURL, defaultNewTab, "newTabURL is reset to default.");
  await AddonTestUtils.promiseShutdownManager();
});

add_task(async function test_settings_validated_safemode() {
  let defaultNewTab = AboutNewTab.newTabURL;
  equal(defaultNewTab, "about:newtab", "Newtab url is default.");
  let defaultHomepageURL = HomePage.get();
  equal(defaultHomepageURL, "about:home", "Home page url is default.");

  function isDefaultSettings(postfix) {
    equal(
      HomePage.get(),
      defaultHomepageURL,
      `Home page url is default ${postfix}.`
    );
    equal(
      AboutNewTab.newTabURL,
      defaultNewTab,
      `newTabURL is default ${postfix}.`
    );
  }

  function isExtensionSettings(postfix) {
    equal(
      HomePage.get(),
      "https://example.com/",
      `Home page url is extension controlled ${postfix}.`
    );
    ok(
      AboutNewTab.newTabURL.endsWith("/newtab"),
      `newTabURL is extension controlled ${postfix}.`
    );
  }

  async function switchSafeMode(inSafeMode) {
    await AddonTestUtils.promiseShutdownManager();
    AddonTestUtils.appInfo.inSafeMode = inSafeMode;
    await AddonTestUtils.promiseStartupManager();
    return AddonManager.getAddonByID("test@mochi");
  }

  let xpi = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "1.0",
      browser_specific_settings: { gecko: { id: "test@mochi" } },
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

  isExtensionSettings("on extension startup");

  // Disable in safemode and verify settings are removed in normal mode.
  let addon = await switchSafeMode(true);
  await addon.disable();
  addon = await switchSafeMode(false);
  isDefaultSettings("after disabling addon during safemode");

  // Enable in safemode and verify settings are back in normal mode.
  addon = await switchSafeMode(true);
  await addon.enable();
  addon = await switchSafeMode(false);
  isExtensionSettings("after enabling addon during safemode");

  // Uninstall in safemode and verify settings are removed in normal mode.
  addon = await switchSafeMode(true);
  await addon.uninstall();
  addon = await switchSafeMode(false);
  isDefaultSettings("after uninstalling addon during safemode");

  await AddonTestUtils.promiseShutdownManager();
  await AddonTestUtils.cleanupTempXPIs();
});

// There are more settings modules than used in this test file, they should have been
// loaded during the test extensions uninstall.  Ensure that all settings modules have
// been loaded.
add_task(async function test_settings_modules_loaded() {
  // Test that all settings modules are loaded.
  let modules = Array.from(ExtensionParent.apiManager.settingsModules);
  ok(modules.length, "we have settings modules");
  for (let name of modules) {
    ok(ExtensionParent.apiManager.getModule(name).loaded, `${name} was loaded`);
  }
});
