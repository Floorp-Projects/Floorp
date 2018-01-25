/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://testing-common/AddonTestUtils.jsm");

const {
  createAppInfo,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

add_task(async function test_overrides_update_removal() {
  /* This tests the scenario where the manifest key for homepage and/or
   * search_provider are removed between updates and therefore the
   * settings are expected to revert. */

  const EXTENSION_ID = "test_overrides_update@tests.mozilla.org";
  const HOMEPAGE_URI = "webext-homepage-1.html";

  const HOMEPAGE_URL_PREF = "browser.startup.homepage";

  const getHomePageURL = () => {
    return Services.prefs.getComplexValue(
      HOMEPAGE_URL_PREF, Ci.nsIPrefLocalizedString).data;
  };

  function promisePrefChanged(value) {
    return new Promise((resolve, reject) => {
      Services.prefs.addObserver(HOMEPAGE_URL_PREF, function observer() {
        if (getHomePageURL().endsWith(value)) {
          Services.prefs.removeObserver(HOMEPAGE_URL_PREF, observer);
          resolve();
        }
      });
    });
  }

  await promiseStartupManager();

  let extensionInfo = {
    useAddonManager: "permanent",
    manifest: {
      "version": "1.0",
      "applications": {
        "gecko": {
          "id": EXTENSION_ID,
        },
      },
      "chrome_settings_overrides": {
        "homepage": HOMEPAGE_URI,
        "search_provider": {
          "name": "DuckDuckGo",
          "search_url": "https://example.com/?q={searchTerms}",
          "is_default": true,
        },
      },
    },
  };
  let extension = ExtensionTestUtils.loadExtension(extensionInfo);

  let defaultHomepageURL = getHomePageURL();
  let defaultEngineName = Services.search.currentEngine.name;

  let prefPromise = promisePrefChanged(HOMEPAGE_URI);
  await extension.startup();
  await prefPromise;

  equal(extension.version, "1.0", "The installed addon has the expected version.");
  ok(getHomePageURL().endsWith(HOMEPAGE_URI),
     "Home page url is overriden by the extension.");
  equal(Services.search.currentEngine.name,
        "DuckDuckGo",
        "Default engine is overriden by the extension");

  extensionInfo.manifest = {
    "version": "2.0",
    "applications": {
      "gecko": {
        "id": EXTENSION_ID,
      },
    },
  };

  prefPromise = promisePrefChanged(defaultHomepageURL);
  await extension.upgrade(extensionInfo);
  await prefPromise;

  equal(extension.version, "2.0", "The updated addon has the expected version.");
  equal(getHomePageURL(),
        defaultHomepageURL,
        "Home page url reverted to the default after update.");
  equal(Services.search.currentEngine.name,
        defaultEngineName,
        "Default engine reverted to the default after update.");

  await extension.unload();

  await promiseShutdownManager();
});
