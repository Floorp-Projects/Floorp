/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  HomePage: "resource:///modules/HomePage.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  sinon: "resource://testing-common/Sinon.jsm",
});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);
// Override ExtensionXPCShellUtils.jsm's overriding of the pref as the
// search service needs it.
Services.prefs.clearUserPref("services.settings.default_bucket");

async function setupRemoteSettings() {
  const settings = await RemoteSettings("hijack-blocklists");
  sinon.stub(settings, "get").returns([
    {
      id: "homepage-urls",
      matches: ["ignore=me"],
      _status: "synced",
    },
  ]);
}

function promisePrefChanged(expectedValue) {
  return TestUtils.waitForPrefChange("browser.startup.homepage", value =>
    value.endsWith(expectedValue)
  );
}

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
  await setupRemoteSettings();
});

add_task(async function test_overrides_update_removal() {
  /* This tests the scenario where the manifest key for homepage and/or
   * search_provider are removed between updates and therefore the
   * settings are expected to revert.  It also tests that an extension
   * can make a builtin extension the default extension without user
   * interaction.  */

  const EXTENSION_ID = "test_overrides_update@tests.mozilla.org";
  const HOMEPAGE_URI = "webext-homepage-1.html";

  let extensionInfo = {
    useAddonManager: "permanent",
    manifest: {
      version: "1.0",
      applications: {
        gecko: {
          id: EXTENSION_ID,
        },
      },
      chrome_settings_overrides: {
        homepage: HOMEPAGE_URI,
        search_provider: {
          name: "DuckDuckGo",
          search_url: "https://example.com/?q={searchTerms}",
          is_default: true,
        },
      },
    },
  };
  let extension = ExtensionTestUtils.loadExtension(extensionInfo);

  let defaultHomepageURL = HomePage.get();
  let defaultEngineName = (await Services.search.getDefault()).name;
  ok(defaultEngineName !== "DuckDuckGo", "Default engine is not DuckDuckGo.");

  let prefPromise = promisePrefChanged(HOMEPAGE_URI);
  await extension.startup();
  await AddonTestUtils.waitForSearchProviderStartup(extension);
  await prefPromise;

  equal(
    extension.version,
    "1.0",
    "The installed addon has the expected version."
  );
  ok(
    HomePage.get().endsWith(HOMEPAGE_URI),
    "Home page url is overridden by the extension."
  );
  equal(
    (await Services.search.getDefault()).name,
    "DuckDuckGo",
    "Builtin default engine was set default by extension"
  );

  extensionInfo.manifest = {
    version: "2.0",
    applications: {
      gecko: {
        id: EXTENSION_ID,
      },
    },
  };

  prefPromise = promisePrefChanged(defaultHomepageURL);
  await extension.upgrade(extensionInfo);
  await prefPromise;

  equal(
    extension.version,
    "2.0",
    "The updated addon has the expected version."
  );
  equal(
    HomePage.get(),
    defaultHomepageURL,
    "Home page url reverted to the default after update."
  );
  equal(
    (await Services.search.getDefault()).name,
    defaultEngineName,
    "Default engine reverted to the default after update."
  );

  await extension.unload();
});

add_task(async function test_overrides_update_adding() {
  /* This tests the scenario where an addon adds support for
   * a homepage or search service when upgrading. Neither
   * should override existing entries for those when added
   * in an upgrade. Also, a search_provider being added
   * with is_default should not prompt the user or override
   * the current default engine. */

  const EXTENSION_ID = "test_overrides_update@tests.mozilla.org";
  const HOMEPAGE_URI = "webext-homepage-1.html";

  let extensionInfo = {
    useAddonManager: "permanent",
    manifest: {
      version: "1.0",
      applications: {
        gecko: {
          id: EXTENSION_ID,
        },
      },
    },
  };
  let extension = ExtensionTestUtils.loadExtension(extensionInfo);

  let defaultHomepageURL = HomePage.get();
  let defaultEngineName = (await Services.search.getDefault()).name;
  ok(defaultEngineName !== "DuckDuckGo", "Home page url is not DuckDuckGo.");

  await extension.startup();

  equal(
    extension.version,
    "1.0",
    "The installed addon has the expected version."
  );
  equal(
    HomePage.get(),
    defaultHomepageURL,
    "Home page url is the default after startup."
  );
  equal(
    (await Services.search.getDefault()).name,
    defaultEngineName,
    "Default engine is the default after startup."
  );

  extensionInfo.manifest = {
    version: "2.0",
    applications: {
      gecko: {
        id: EXTENSION_ID,
      },
    },
    chrome_settings_overrides: {
      homepage: HOMEPAGE_URI,
      search_provider: {
        name: "DuckDuckGo",
        search_url: "https://example.com/?q={searchTerms}",
        is_default: true,
      },
    },
  };

  let prefPromise = promisePrefChanged(HOMEPAGE_URI);
  await extension.upgrade(extensionInfo);
  await AddonTestUtils.waitForSearchProviderStartup(extension);
  await prefPromise;

  equal(
    extension.version,
    "2.0",
    "The updated addon has the expected version."
  );
  ok(
    HomePage.get().endsWith(HOMEPAGE_URI),
    "Home page url is overridden by the extension during upgrade."
  );
  // An upgraded extension adding a search engine cannot override
  // the default engine.
  equal(
    (await Services.search.getDefault()).name,
    defaultEngineName,
    "Default engine is still the default after startup."
  );

  await extension.unload();
});

add_task(async function test_overrides_update_homepage_change() {
  /* This tests the scenario where an addon changes
   * a homepage url when upgrading. */

  const EXTENSION_ID = "test_overrides_update@tests.mozilla.org";
  const HOMEPAGE_URI = "webext-homepage-1.html";
  const HOMEPAGE_URI_2 = "webext-homepage-2.html";

  let extensionInfo = {
    useAddonManager: "permanent",
    manifest: {
      version: "1.0",
      applications: {
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

  let prefPromise = promisePrefChanged(HOMEPAGE_URI);
  await extension.startup();
  await prefPromise;

  equal(
    extension.version,
    "1.0",
    "The installed addon has the expected version."
  );
  ok(
    HomePage.get().endsWith(HOMEPAGE_URI),
    "Home page url is the extension url after startup."
  );

  extensionInfo.manifest = {
    version: "2.0",
    applications: {
      gecko: {
        id: EXTENSION_ID,
      },
    },
    chrome_settings_overrides: {
      homepage: HOMEPAGE_URI_2,
    },
  };

  prefPromise = promisePrefChanged(HOMEPAGE_URI_2);
  await extension.upgrade(extensionInfo);
  await prefPromise;

  equal(
    extension.version,
    "2.0",
    "The updated addon has the expected version."
  );
  ok(
    HomePage.get().endsWith(HOMEPAGE_URI_2),
    "Home page url is by the extension after upgrade."
  );

  await extension.unload();
});
