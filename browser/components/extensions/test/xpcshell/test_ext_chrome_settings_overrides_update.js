/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {AddonTestUtils} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");
const {HomePage} = ChromeUtils.import("resource:///modules/HomePage.jsm");

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

AddonTestUtils.createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");


const HOMEPAGE_URL_PREF = "browser.startup.homepage";

function promisePrefChanged(value) {
  return new Promise((resolve, reject) => {
    Services.prefs.addObserver(HOMEPAGE_URL_PREF, function observer() {
      if (HomePage.get().endsWith(value)) {
        Services.prefs.removeObserver(HOMEPAGE_URL_PREF, observer);
        resolve();
      }
    });
  });
}

add_task(async function startup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_overrides_update_removal() {
  /* This tests the scenario where the manifest key for homepage and/or
   * search_provider are removed between updates and therefore the
   * settings are expected to revert. */

  const EXTENSION_ID = "test_overrides_update@tests.mozilla.org";
  const HOMEPAGE_URI = "webext-homepage-1.html";

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

  let defaultHomepageURL = HomePage.get();
  let defaultEngineName = (await Services.search.getDefault()).name;

  let prefPromise = promisePrefChanged(HOMEPAGE_URI);
  await extension.startup();
  await AddonTestUtils.waitForSearchProviderStartup(extension);
  await prefPromise;

  equal(extension.version, "1.0", "The installed addon has the expected version.");
  ok(HomePage.get().endsWith(HOMEPAGE_URI),
     "Home page url is overridden by the extension.");
  let engine = await Services.search.getDefault();
  equal(engine.name, "DuckDuckGo",
        "Default engine is overridden by the extension");
  // Extensions cannot change a built-in search engine.
  let url = engine._getURLOfType("text/html").template;
  equal(url, "https://duckduckgo.com/",
        "Extension cannot override default engine search template.");

  // test changing the homepage
  extensionInfo.manifest = {
    "version": "2.0",
    "applications": {
      "gecko": {
        "id": EXTENSION_ID,
      },
    },
    "chrome_settings_overrides": {
      "homepage": "webext-homepage-2.html",
      "search_provider": {
        "name": "DuckDuckGo",
        "search_url": "https://example.org/?q={searchTerms}",
        "is_default": true,
      },
    },
  };

  prefPromise = promisePrefChanged("webext-homepage-2.html");
  await extension.upgrade(extensionInfo);
  await AddonTestUtils.waitForSearchProviderStartup(extension);
  await prefPromise;

  equal(extension.version, "2.0", "The installed addon has the expected version.");
  ok(HomePage.get().endsWith("webext-homepage-2.html"),
     "Home page url is overridden by the extension.");
  engine = await Services.search.getDefault();
  equal(engine.name, "DuckDuckGo",
        "Default engine is overridden by the extension");
  url = engine._getURLOfType("text/html").template;
  // Extensions cannot change a built-in search engine.
  equal(url, "https://duckduckgo.com/",
        "Default engine is overridden by the extension");

  extensionInfo.manifest = {
    "version": "3.0",
    "applications": {
      "gecko": {
        "id": EXTENSION_ID,
      },
    },
  };

  prefPromise = promisePrefChanged(defaultHomepageURL);
  await extension.upgrade(extensionInfo);
  await prefPromise;

  equal(extension.version, "3.0", "The updated addon has the expected version.");
  equal(HomePage.get(),
        defaultHomepageURL,
        "Home page url reverted to the default after update.");
  equal((await Services.search.getDefault()).name,
        defaultEngineName,
        "Default engine reverted to the default after update.");

  await extension.unload();
});

add_task(async function test_overrides_update_adding() {
  /* This tests the scenario where an addon adds support for
   * a homepage or search service when upgrading. Neither
   * should override existing entries for those when added
   * in an upgrade. */

  const EXTENSION_ID = "test_overrides_update@tests.mozilla.org";
  const HOMEPAGE_URI = "webext-homepage-1.html";

  let extensionInfo = {
    useAddonManager: "permanent",
    manifest: {
      "version": "1.0",
      "applications": {
        "gecko": {
          "id": EXTENSION_ID,
        },
      },
    },
  };
  let extension = ExtensionTestUtils.loadExtension(extensionInfo);

  let defaultHomepageURL = HomePage.get();
  let defaultEngineName = (await Services.search.getDefault()).name;

  await extension.startup();

  equal(extension.version, "1.0", "The installed addon has the expected version.");
  equal(HomePage.get(),
        defaultHomepageURL,
        "Home page url is the default after startup.");
  equal((await Services.search.getDefault()).name,
        defaultEngineName,
        "Default engine is the default after startup.");

  extensionInfo.manifest = {
    "version": "2.0",
    "applications": {
      "gecko": {
        "id": EXTENSION_ID,
      },
    },
    "chrome_settings_overrides": {
      "homepage": HOMEPAGE_URI,
      "search_provider": {
        "name": "MozSearch",
        "search_url": "https://example.com/?q={searchTerms}",
        "is_default": true,
      },
    },
  };

  await extension.upgrade(extensionInfo);
  // The homepage pref shouldn't change here, we delay a tick to give
  // a pref change a chance in case it were to happen.
  await Promise.all([
    AddonTestUtils.waitForSearchProviderStartup(extension),
    delay(),
  ]);

  equal(extension.version, "2.0", "The updated addon has the expected version.");
  ok(HomePage.get().endsWith(defaultHomepageURL),
     "Home page url is not overridden by the extension during upgrade.");
  ok(!!Services.search.getEngineByName("MozSearch"),
     "Engine was installed by extension");
  // An upgraded extension adding a search engine cannot override
  // the default engine.
  equal((await Services.search.getDefault()).name,
        defaultEngineName,
        "Default engine is the default after startup.");

  await extension.unload();
});

add_task(async function test_overrides_update_changing() {
  /* This tests the scenario where the homepage url changes
   * due to the upgrade. */

  const EXTENSION_ID = "test_overrides_changing@tests.mozilla.org";
  const HOMEPAGE_URI = "webext-homepage-1.html";

  let extensionInfo = {
    useAddonManager: "temporary",
    manifest: {
      "version": "1.0",
      "applications": {
        "gecko": {
          "id": EXTENSION_ID,
        },
      },
      "chrome_settings_overrides": {
        "homepage": HOMEPAGE_URI,
      },
    },
  };
  let extension = ExtensionTestUtils.loadExtension(extensionInfo);

  let prefPromise = promisePrefChanged(HOMEPAGE_URI);
  await extension.startup();
  await prefPromise;

  equal(extension.version, "1.0", "The installed addon has the expected version.");
  ok(HomePage.get().endsWith(HOMEPAGE_URI),
     "Home page url is the default after startup.");

  extensionInfo.manifest = {
    "version": "2.0",
    "applications": {
      "gecko": {
        "id": EXTENSION_ID,
      },
    },
    "chrome_settings_overrides": {
      "homepage": HOMEPAGE_URI + ".2",
    },
  };

  prefPromise = promisePrefChanged(HOMEPAGE_URI + ".2");
  await extension.upgrade(extensionInfo);
  await prefPromise;

  equal(extension.version, "2.0", "The updated addon has the expected version.");
  ok(HomePage.get().endsWith(HOMEPAGE_URI + ".2"),
     "Home page url is not overridden by the extension during upgrade.");

  await extension.unload();
});
