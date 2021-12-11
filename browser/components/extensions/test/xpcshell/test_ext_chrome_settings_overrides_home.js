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
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
});

function promisePrefChanged(expectedValue) {
  return TestUtils.waitForPrefChange("browser.startup.homepage", value =>
    value.endsWith(expectedValue)
  );
}

const HOMEPAGE_EXTENSION_CONTROLLED =
  "browser.startup.homepage_override.extensionControlled";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

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

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
  await setupRemoteSettings();
});

add_task(async function test_overriding_with_ignored_url() {
  // Manually poke into the ignore list a value to be ignored.
  HomePage._ignoreList.push("ignore=me");
  Services.prefs.setBoolPref(HOMEPAGE_EXTENSION_CONTROLLED, false);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: {
          id: "ignore_homepage@example.com",
        },
      },
      chrome_settings_overrides: { homepage: "https://example.com/?ignore=me" },
      name: "extension",
    },
    useAddonManager: "temporary",
  });

  await extension.startup();

  Assert.ok(HomePage.isDefault, "Should still have the default homepage");
  Assert.equal(
    Services.prefs.getBoolPref(
      "browser.startup.homepage_override.extensionControlled"
    ),
    false,
    "Should not be extension controlled."
  );
  TelemetryTestUtils.assertEvents(
    [
      {
        object: "ignore",
        value: "set_blocked_extension",
        extra: { webExtensionId: "ignore_homepage@example.com" },
      },
    ],
    {
      category: "homepage",
      method: "preference",
    }
  );

  await extension.unload();
  HomePage._ignoreList.pop();
});

add_task(async function test_overriding_cancelled_after_ignore_update() {
  const oldHomePageIgnoreList = HomePage._ignoreList;
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: {
          id: "ignore_homepage1@example.com",
        },
      },
      chrome_settings_overrides: {
        homepage: "https://example.com/?ignore1=me",
      },
      name: "extension",
    },
    useAddonManager: "temporary",
  });

  await extension.startup();

  Assert.ok(!HomePage.isDefault, "Should have overriden the new homepage");
  Assert.equal(
    Services.prefs.getBoolPref(
      "browser.startup.homepage_override.extensionControlled"
    ),
    true,
    "Should be extension controlled."
  );

  let prefChanged = TestUtils.waitForPrefChange(
    "browser.startup.homepage_override.extensionControlled"
  );

  await HomePage._handleIgnoreListUpdated({
    data: {
      current: [{ id: "homepage-urls", matches: ["ignore1=me"] }],
    },
  });

  await prefChanged;

  await TestUtils.waitForCondition(
    () =>
      !Services.prefs.getBoolPref(
        "browser.startup.homepage_override.extensionControlled",
        false
      ),
    "Should not longer be extension controlled"
  );

  Assert.ok(HomePage.isDefault, "Should have reset the homepage");

  TelemetryTestUtils.assertEvents(
    [
      {
        object: "ignore",
        value: "saved_reset",
      },
    ],
    {
      category: "homepage",
      method: "preference",
    }
  );

  await extension.unload();
  HomePage._ignoreList = oldHomePageIgnoreList;
});

add_task(async function test_overriding_homepage_locale() {
  Services.locale.availableLocales = ["en-US", "es-ES"];

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: "homepage@example.com",
        },
      },
      chrome_settings_overrides: {
        homepage: "/__MSG_homepage__",
      },
      name: "extension",
      default_locale: "en",
    },
    useAddonManager: "permanent",

    files: {
      "_locales/en/messages.json": {
        homepage: {
          message: "homepage.html",
          description: "homepage",
        },
      },

      "_locales/es_ES/messages.json": {
        homepage: {
          message: "default.html",
          description: "homepage",
        },
      },
    },
  });

  let prefPromise = promisePrefChanged("homepage.html");
  await extension.startup();
  await prefPromise;

  Assert.equal(
    HomePage.get(),
    `moz-extension://${extension.uuid}/homepage.html`,
    "Should have overridden the new homepage"
  );

  // Set the new locale now, and disable the L10nRegistry reset
  // when shutting down the addon mananger.  This allows us to
  // restart under a new locale without a lot of fuss.
  let reqLoc = Services.locale.requestedLocales;
  Services.locale.requestedLocales = ["es-ES"];

  prefPromise = promisePrefChanged("default.html");
  await AddonTestUtils.promiseShutdownManager({ clearL10nRegistry: false });
  await AddonTestUtils.promiseStartupManager();
  await extension.awaitStartup();
  await prefPromise;

  Assert.equal(
    HomePage.get(),
    `moz-extension://${extension.uuid}/default.html`,
    "Should have overridden the new homepage"
  );

  await extension.unload();

  Services.locale.requestedLocales = reqLoc;
});
