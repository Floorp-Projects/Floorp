/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

// Named this way so they correspond to the extensions
const HOME_URI_2 = "http://example.com/";
const HOME_URI_3 = "http://example.org/";
const HOME_URI_4 = "http://example.net/";

const CONTROLLED_BY_THIS = "controlled_by_this_extension";
const CONTROLLED_BY_OTHER = "controlled_by_other_extensions";
const NOT_CONTROLLABLE = "not_controllable";

const HOMEPAGE_URL_PREF = "browser.startup.homepage";

const getHomePageURL = () => {
  return Services.prefs.getComplexValue(
    HOMEPAGE_URL_PREF, Ci.nsIPrefLocalizedString).data;
};

add_task(async function test_multiple_extensions_overriding_home_page() {
  let defaultHomePage = getHomePageURL();

  function background() {
    browser.test.onMessage.addListener(async msg => {
      switch (msg) {
        case "checkHomepage":
          let homepage = await browser.browserSettings.homepageOverride.get({});
          browser.test.sendMessage("homepage", homepage);
          break;
        case "trySet":
          let setResult = await browser.browserSettings.homepageOverride.set({value: "foo"});
          browser.test.assertFalse(setResult, "Calling homepageOverride.set returns false.");
          browser.test.sendMessage("homepageSet");
          break;
        case "tryClear":
          let clearResult = await browser.browserSettings.homepageOverride.clear({});
          browser.test.assertFalse(clearResult, "Calling homepageOverride.clear returns false.");
          browser.test.sendMessage("homepageCleared");
          break;
      }
    });
  }

  let extObj = {
    manifest: {
      "chrome_settings_overrides": {},
      permissions: ["browserSettings"],
    },
    useAddonManager: "temporary",
    background,
  };

  let ext1 = ExtensionTestUtils.loadExtension(extObj);

  extObj.manifest.chrome_settings_overrides = {homepage: HOME_URI_2};
  let ext2 = ExtensionTestUtils.loadExtension(extObj);

  extObj.manifest.chrome_settings_overrides = {homepage: HOME_URI_3};
  let ext3 = ExtensionTestUtils.loadExtension(extObj);

  extObj.manifest.chrome_settings_overrides = {homepage: HOME_URI_4};
  let ext4 = ExtensionTestUtils.loadExtension(extObj);

  extObj.manifest.chrome_settings_overrides = {};
  let ext5 = ExtensionTestUtils.loadExtension(extObj);

  async function checkHomepageOverride(ext, expectedValue, expectedLevelOfControl) {
    ext.sendMessage("checkHomepage");
    let homepage = await ext.awaitMessage("homepage");
    is(homepage.value, expectedValue,
       `homepageOverride setting returns the expected value: ${expectedValue}.`);
    is(homepage.levelOfControl, expectedLevelOfControl,
       `homepageOverride setting returns the expected levelOfControl: ${expectedLevelOfControl}.`);
  }

  await ext1.startup();

  is(getHomePageURL(), defaultHomePage,
     "Home url should be the default");
  await checkHomepageOverride(ext1, getHomePageURL(), NOT_CONTROLLABLE);

  // Because we are expecting the pref to change when we start or unload, we
  // need to wait on a pref change.  This is because the pref management is
  // async and can happen after the startup/unload is finished.
  let prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext2.startup();
  await prefPromise;

  ok(getHomePageURL().endsWith(HOME_URI_2),
     "Home url should be overridden by the second extension.");

  await checkHomepageOverride(ext1, HOME_URI_2, CONTROLLED_BY_OTHER);

  // Verify that calling set and clear do nothing.
  ext2.sendMessage("trySet");
  await ext2.awaitMessage("homepageSet");
  await checkHomepageOverride(ext1, HOME_URI_2, CONTROLLED_BY_OTHER);

  ext2.sendMessage("tryClear");
  await ext2.awaitMessage("homepageCleared");
  await checkHomepageOverride(ext1, HOME_URI_2, CONTROLLED_BY_OTHER);

  // Because we are unloading an earlier extension, browser.startup.homepage won't change
  await ext1.unload();

  await checkHomepageOverride(ext2, HOME_URI_2, CONTROLLED_BY_THIS);

  ok(getHomePageURL().endsWith(HOME_URI_2),
     "Home url should be overridden by the second extension.");

  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext3.startup();
  await prefPromise;

  ok(getHomePageURL().endsWith(HOME_URI_3),
     "Home url should be overridden by the third extension.");

  await checkHomepageOverride(ext3, HOME_URI_3, CONTROLLED_BY_THIS);

  // Because we are unloading an earlier extension, browser.startup.homepage won't change
  await ext2.unload();

  ok(getHomePageURL().endsWith(HOME_URI_3),
     "Home url should be overridden by the third extension.");

  await checkHomepageOverride(ext3, HOME_URI_3, CONTROLLED_BY_THIS);

  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext4.startup();
  await prefPromise;

  ok(getHomePageURL().endsWith(HOME_URI_4),
     "Home url should be overridden by the third extension.");

  await checkHomepageOverride(ext3, HOME_URI_4, CONTROLLED_BY_OTHER);

  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext4.unload();
  await prefPromise;

  ok(getHomePageURL().endsWith(HOME_URI_3),
     "Home url should be overridden by the third extension.");

  await checkHomepageOverride(ext3, HOME_URI_3, CONTROLLED_BY_THIS);

  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext3.unload();
  await prefPromise;

  is(getHomePageURL(), defaultHomePage,
     "Home url should be reset to default");

  await ext5.startup();
  await checkHomepageOverride(ext5, defaultHomePage, NOT_CONTROLLABLE);
  await ext5.unload();
});

const HOME_URI_1 = "http://example.com/";
const USER_URI = "http://example.edu/";

add_task(async function test_extension_setting_home_page_back() {
  let defaultHomePage = getHomePageURL();

  Services.prefs.setStringPref(HOMEPAGE_URL_PREF, USER_URI);

  is(getHomePageURL(), USER_URI,
     "Home url should be the user set value");

  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_settings_overrides": {homepage: HOME_URI_1}},
    useAddonManager: "temporary",
  });

  // Because we are expecting the pref to change when we start or unload, we
  // need to wait on a pref change.  This is because the pref management is
  // async and can happen after the startup/unload is finished.
  let prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext1.startup();
  await prefPromise;

  ok(getHomePageURL().endsWith(HOME_URI_1),
     "Home url should be overridden by the second extension.");

  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext1.unload();
  await prefPromise;

  is(getHomePageURL(), USER_URI,
     "Home url should be the user set value");

  Services.prefs.clearUserPref(HOMEPAGE_URL_PREF);

  is(getHomePageURL(), defaultHomePage,
     "Home url should be the default");
});

add_task(async function test_disable() {
  const ID = "id@tests.mozilla.org";
  let defaultHomePage = getHomePageURL();

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

  let prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext1.startup();
  await prefPromise;

  is(getHomePageURL(), HOME_URI_1,
     "Home url should be overridden by the extension.");

  let addon = await AddonManager.getAddonByID(ID);
  is(addon.id, ID, "Found the correct add-on.");

  let disabledPromise = awaitEvent("shutdown", ID);
  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  addon.userDisabled = true;
  await Promise.all([disabledPromise, prefPromise]);

  is(getHomePageURL(), defaultHomePage,
     "Home url should be the default");

  let enabledPromise = awaitEvent("ready", ID);
  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  addon.userDisabled = false;
  await Promise.all([enabledPromise, prefPromise]);

  is(getHomePageURL(), HOME_URI_1,
     "Home url should be overridden by the extension.");

  prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext1.unload();
  await prefPromise;

  is(getHomePageURL(), defaultHomePage,
     "Home url should be the default");
});

add_task(async function test_local() {
  let ext1 = ExtensionTestUtils.loadExtension({
    manifest: {"chrome_settings_overrides": {"homepage": "home.html"}},
    useAddonManager: "temporary",
  });

  let prefPromise = promisePrefChangeObserved(HOMEPAGE_URL_PREF);
  await ext1.startup();
  await prefPromise;

  let homepage = getHomePageURL();
  ok((homepage.startsWith("moz-extension") && homepage.endsWith("home.html")),
     "Home url should be relative to extension.");

  await ext1.unload();
});
