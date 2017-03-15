/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");

// Named this way so they correspond to the extensions
const HOME_URI_2 = "http://example.com/";
const HOME_URI_3 = "http://example.org/";
const HOME_URI_4 = "http://example.net/";

add_task(function* test_multiple_extensions_overriding_home_page() {
  let defaultHomePage = Preferences.get("browser.startup.homepage");

  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_settings_overrides": {}},
    useAddonManager: "temporary",
  });

  let ext2 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_settings_overrides": {homepage: HOME_URI_2}},
    useAddonManager: "temporary",
  });

  let ext3 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_settings_overrides": {homepage: HOME_URI_3}},
    useAddonManager: "temporary",
  });

  let ext4 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_settings_overrides": {homepage: HOME_URI_4}},
    useAddonManager: "temporary",
  });

  yield ext1.startup();

  is(Preferences.get("browser.startup.homepage"), defaultHomePage,
     "Home url should be the default");

  // Because we are expecting the pref to change when we start or unload, we
  // need to wait on a pref change.  This is because the pref management is
  // async and can happen after the startup/unload is finished.
  let prefPromise = promisePrefChangeObserved("browser.startup.homepage");
  yield ext2.startup();
  yield prefPromise;

  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI_2),
     "Home url should be overridden by the second extension.");

  // Because we are unloading an earlier extension, browser.startup.homepage won't change
  yield ext1.unload();

  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI_2),
     "Home url should be overridden by the second extension.");

  prefPromise = promisePrefChangeObserved("browser.startup.homepage");
  yield ext3.startup();
  yield prefPromise;

  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI_3),
     "Home url should be overridden by the third extension.");

  // Because we are unloading an earlier extension, browser.startup.homepage won't change
  yield ext2.unload();

  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI_3),
     "Home url should be overridden by the third extension.");

  prefPromise = promisePrefChangeObserved("browser.startup.homepage");
  yield ext4.startup();
  yield prefPromise;

  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI_4),
     "Home url should be overridden by the third extension.");


  prefPromise = promisePrefChangeObserved("browser.startup.homepage");
  yield ext4.unload();
  yield prefPromise;

  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI_3),
     "Home url should be overridden by the third extension.");

  prefPromise = promisePrefChangeObserved("browser.startup.homepage");
  yield ext3.unload();
  yield prefPromise;

  is(Preferences.get("browser.startup.homepage"), defaultHomePage,
     "Home url should be reset to default");
});

const HOME_URI_1 = "http://example.com/";
const USER_URI = "http://example.edu/";

add_task(function* test_extension_setting_home_page_back() {
  let defaultHomePage = Preferences.get("browser.startup.homepage");

  Preferences.set("browser.startup.homepage", USER_URI);

  is(Preferences.get("browser.startup.homepage"), USER_URI,
     "Home url should be the user set value");

  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_settings_overrides": {homepage: HOME_URI_1}},
    useAddonManager: "temporary",
  });

  // Because we are expecting the pref to change when we start or unload, we
  // need to wait on a pref change.  This is because the pref management is
  // async and can happen after the startup/unload is finished.
  let prefPromise = promisePrefChangeObserved("browser.startup.homepage");
  yield ext1.startup();
  yield prefPromise;

  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI_1),
     "Home url should be overridden by the second extension.");

  prefPromise = promisePrefChangeObserved("browser.startup.homepage");
  yield ext1.unload();
  yield prefPromise;

  is(Preferences.get("browser.startup.homepage"), USER_URI,
     "Home url should be the user set value");

  Preferences.reset("browser.startup.homepage");

  is(Preferences.get("browser.startup.homepage"), defaultHomePage,
     "Home url should be the default");
});

add_task(function* test_disable() {
  let defaultHomePage = Preferences.get("browser.startup.homepage");

  const ID = "id@tests.mozilla.org";

  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: ID,
        },
      },
      "chrome_settings_overrides": {
        homepage: HOME_URI_1,
      },
    },
    useAddonManager: "temporary",
  });

  let prefPromise = promisePrefChangeObserved("browser.startup.homepage");
  yield ext1.startup();
  yield prefPromise;

  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI_1),
     "Home url should be overridden by the extension.");

  let addon = yield AddonManager.getAddonByID(ID);
  is(addon.id, ID);

  prefPromise = promisePrefChangeObserved("browser.startup.homepage");
  addon.userDisabled = true;
  yield prefPromise;

  is(Preferences.get("browser.startup.homepage"), defaultHomePage,
     "Home url should be the default");

  prefPromise = promisePrefChangeObserved("browser.startup.homepage");
  addon.userDisabled = false;
  yield prefPromise;

  ok(Preferences.get("browser.startup.homepage").endsWith(HOME_URI_1),
     "Home url should be overridden by the extension.");

  prefPromise = promisePrefChangeObserved("browser.startup.homepage");
  yield ext1.unload();
  yield prefPromise;

  is(Preferences.get("browser.startup.homepage"), defaultHomePage,
     "Home url should be the default");
});
