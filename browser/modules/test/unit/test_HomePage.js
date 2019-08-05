/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  HomePage: "resource:///modules/HomePage.jsm",
  Services: "resource://gre/modules/Services.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  // RemoteSettingsClient: "resource://services-settings/RemoteSettingsClient.jsm",
  sinon: "resource://testing-common/Sinon.jsm",
});

const HOMEPAGE_IGNORELIST = "homepage-urls";

/**
 * Provides a basic set of remote settings for use in tests.
 */
async function setupRemoteSettings() {
  const settings = await RemoteSettings("hijack-blocklists");
  sinon.stub(settings, "get").returns([
    {
      id: HOMEPAGE_IGNORELIST,
      matches: ["ignore=me"],
      _status: "synced",
    },
  ]);
}

add_task(async function setup() {
  await setupRemoteSettings();
});

add_task(function test_HomePage() {
  Assert.ok(
    !HomePage.overridden,
    "Homepage should not be overriden by default."
  );
  let newvalue = "about:blank|about:newtab";
  HomePage.set(newvalue);
  Assert.ok(HomePage.overridden, "Homepage should be overriden after set()");
  Assert.equal(HomePage.get(), newvalue, "Homepage should be ${newvalue}");
  Assert.notEqual(
    HomePage.getDefault(),
    newvalue,
    "Homepage should be ${newvalue}"
  );
  HomePage.reset();
  Assert.ok(
    !HomePage.overridden,
    "Homepage should not be overriden by after reset."
  );
  Assert.equal(
    HomePage.get(),
    HomePage.getDefault(),
    "Homepage and default should be equal after reset."
  );
});

add_task(function test_readLocalizedHomepage() {
  let newvalue = "data:text/plain,browser.startup.homepage%3Dabout%3Alocalized";
  let complexvalue = Cc["@mozilla.org/pref-localizedstring;1"].createInstance(
    Ci.nsIPrefLocalizedString
  );
  complexvalue.data = newvalue;
  Services.prefs
    .getDefaultBranch(null)
    .setComplexValue(
      "browser.startup.homepage",
      Ci.nsIPrefLocalizedString,
      complexvalue
    );
  Assert.ok(!HomePage.overridden, "Complex value only works as default");
  Assert.equal(HomePage.get(), "about:localized", "Get value from bundle");
});

add_task(function test_recoverEmptyHomepage() {
  Assert.ok(
    !HomePage.overridden,
    "Homepage should not be overriden by default."
  );
  Services.prefs.setStringPref("browser.startup.homepage", "");
  Assert.ok(HomePage.overridden, "Homepage is overriden with empty string.");
  Assert.equal(HomePage.get(), HomePage.getDefault(), "Recover is default");
  Assert.ok(!HomePage.overridden, "Recover should have set default");
});

add_task(async function test_initWithIgnoredPageCausesReset() {
  HomePage.set("http://bad/?ignore=me");
  Assert.ok(HomePage.overridden, "Should have overriden the homepage");

  await HomePage.init();

  Assert.ok(
    !HomePage.overridden,
    "Should no longer be overriding the homepage."
  );
  Assert.equal(
    HomePage.get(),
    HomePage.getDefault(),
    "Should have reset to the default preference"
  );
});

add_task(async function test_updateIgnoreListCausesReset() {
  HomePage.set("http://bad/?new=ignore");
  Assert.ok(HomePage.overridden, "Should have overriden the homepage");

  // Simulate an ignore list update.
  await RemoteSettings("hijack-blocklists").emit("sync", {
    data: {
      current: [
        {
          id: HOMEPAGE_IGNORELIST,
          schema: 1553857697843,
          last_modified: 1553859483588,
          matches: ["ignore=me", "new=ignore"],
        },
      ],
    },
  });

  Assert.ok(
    !HomePage.overridden,
    "Should no longer be overriding the homepage."
  );
  Assert.equal(
    HomePage.get(),
    HomePage.getDefault(),
    "Should have reset to the default preference"
  );
});
