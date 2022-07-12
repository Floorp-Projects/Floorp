/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  HomePage: "resource:///modules/HomePage.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  sinon: "resource://testing-common/Sinon.jsm",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
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
      matches: ["ignore=me", "ignoreCASE=ME"],
      _status: "synced",
    },
  ]);
}

add_task(async function setup() {
  await setupRemoteSettings();
});

add_task(async function test_initWithIgnoredPageCausesReset() {
  // Set the preference direct as the set() would block us.
  Services.prefs.setStringPref(
    "browser.startup.homepage",
    "http://bad/?ignore=me"
  );
  Assert.ok(HomePage.overridden, "Should have overriden the homepage");

  await HomePage.delayedStartup();

  Assert.ok(
    !HomePage.overridden,
    "Should no longer be overriding the homepage."
  );
  Assert.equal(
    HomePage.get(),
    HomePage.getDefault(),
    "Should have reset to the default preference"
  );

  TelemetryTestUtils.assertEvents(
    [{ object: "ignore", value: "saved_reset" }],
    {
      category: "homepage",
      method: "preference",
    }
  );
});

add_task(async function test_updateIgnoreListCausesReset() {
  Services.prefs.setStringPref(
    "browser.startup.homepage",
    "http://bad/?new=ignore"
  );
  Assert.ok(HomePage.overridden, "Should have overriden the homepage");

  // Simulate an ignore list update.
  await RemoteSettings("hijack-blocklists").emit("sync", {
    data: {
      current: [
        {
          id: HOMEPAGE_IGNORELIST,
          schema: 1553857697843,
          last_modified: 1553859483588,
          matches: ["ignore=me", "ignoreCASE=ME", "new=ignore"],
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
  TelemetryTestUtils.assertEvents(
    [{ object: "ignore", value: "saved_reset" }],
    {
      category: "homepage",
      method: "preference",
    }
  );
});

async function testSetIgnoredUrl(url) {
  Assert.ok(!HomePage.overriden, "Should not be overriding the homepage");

  await HomePage.set(url);

  Assert.equal(
    HomePage.get(),
    HomePage.getDefault(),
    "Should still have the default homepage."
  );
  Assert.ok(!HomePage.overriden, "Should not be overriding the homepage.");
  TelemetryTestUtils.assertEvents(
    [{ object: "ignore", value: "set_blocked" }],
    {
      category: "homepage",
      method: "preference",
    }
  );
}

add_task(async function test_setIgnoredUrl() {
  await testSetIgnoredUrl("http://bad/?ignore=me");
});

add_task(async function test_setIgnoredUrl_case() {
  await testSetIgnoredUrl("http://bad/?Ignorecase=me");
});
