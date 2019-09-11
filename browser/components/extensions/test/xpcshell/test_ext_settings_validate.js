/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { HomePage } = ChromeUtils.import("resource:///modules/HomePage.jsm");

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
});
