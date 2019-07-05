/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "aboutNewTabService",
  "@mozilla.org/browser/aboutnewtab-service;1",
  "nsIAboutNewTabService"
);

const IS_RELEASE_OR_BETA = AppConstants.RELEASE_OR_BETA;

const DOWNLOADS_URL =
  "chrome://browser/content/downloads/contentAreaDownloadsView.xul";
const SEPARATE_PRIVILEGED_CONTENT_PROCESS_PREF =
  "browser.tabs.remote.separatePrivilegedContentProcess";
const ACTIVITY_STREAM_DEBUG_PREF = "browser.newtabpage.activity-stream.debug";

function cleanup() {
  Services.prefs.clearUserPref(SEPARATE_PRIVILEGED_CONTENT_PROCESS_PREF);
  Services.prefs.clearUserPref(ACTIVITY_STREAM_DEBUG_PREF);
  aboutNewTabService.resetNewTabURL();
}

registerCleanupFunction(cleanup);

let ACTIVITY_STREAM_URL;
let ACTIVITY_STREAM_DEBUG_URL;

function setExpectedUrlsWithScripts() {
  ACTIVITY_STREAM_URL =
    "resource://activity-stream/prerendered/activity-stream.html";
  ACTIVITY_STREAM_DEBUG_URL =
    "resource://activity-stream/prerendered/activity-stream-debug.html";
}

function setExpectedUrlsWithoutScripts() {
  ACTIVITY_STREAM_URL =
    "resource://activity-stream/prerendered/activity-stream-noscripts.html";

  // Debug urls are the same as non-debug because debug scripts load dynamically
  ACTIVITY_STREAM_DEBUG_URL = ACTIVITY_STREAM_URL;
}

function nextChangeNotificationPromise(aNewURL, testMessage) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
      // jshint unused:false
      Services.obs.removeObserver(observer, aTopic);
      Assert.equal(aData, aNewURL, testMessage);
      resolve();
    }, "newtab-url-changed");
  });
}

function setPrivilegedContentProcessPref(usePrivilegedContentProcess) {
  if (
    usePrivilegedContentProcess ===
    Services.prefs.getBoolPref(SEPARATE_PRIVILEGED_CONTENT_PROCESS_PREF)
  ) {
    return Promise.resolve();
  }

  let notificationPromise = nextChangeNotificationPromise("about:newtab");
  Services.prefs.setBoolPref(
    SEPARATE_PRIVILEGED_CONTENT_PROCESS_PREF,
    usePrivilegedContentProcess
  );
  return notificationPromise;
}

// Default expected URLs to files with scripts in them.
setExpectedUrlsWithScripts();

function addTestsWithPrivilegedContentProcessPref(test) {
  add_task(async () => {
    await setPrivilegedContentProcessPref(true);
    setExpectedUrlsWithoutScripts();
    await test();
  });
  add_task(async () => {
    await setPrivilegedContentProcessPref(false);
    setExpectedUrlsWithScripts();
    await test();
  });
}

function setBoolPrefAndWaitForChange(pref, value, testMessage) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
      // jshint unused:false
      Services.obs.removeObserver(observer, aTopic);
      Assert.equal(aData, aboutNewTabService.newTabURL, testMessage);
      resolve();
    }, "newtab-url-changed");

    Services.prefs.setBoolPref(pref, value);
  });
}

add_task(async function test_as_initial_values() {
  Assert.ok(
    aboutNewTabService.activityStreamEnabled,
    ".activityStreamEnabled should be set to the correct initial value"
  );
  // This pref isn't defined on release or beta, so we fall back to false
  Assert.equal(
    aboutNewTabService.activityStreamDebug,
    Services.prefs.getBoolPref(ACTIVITY_STREAM_DEBUG_PREF, false),
    ".activityStreamDebug should be set to the correct initial value"
  );
});

/**
 * Test the overriding of the default URL
 */
add_task(async function test_override_activity_stream_disabled() {
  let notificationPromise;

  // override with some remote URL
  let url = "http://example.com/";
  notificationPromise = nextChangeNotificationPromise(url);
  aboutNewTabService.newTabURL = url;
  await notificationPromise;
  Assert.ok(aboutNewTabService.overridden, "Newtab URL should be overridden");
  Assert.ok(
    !aboutNewTabService.activityStreamEnabled,
    "Newtab activity stream should not be enabled"
  );
  Assert.equal(
    aboutNewTabService.newTabURL,
    url,
    "Newtab URL should be the custom URL"
  );

  // test reset with activity stream disabled
  notificationPromise = nextChangeNotificationPromise("about:newtab");
  aboutNewTabService.resetNewTabURL();
  await notificationPromise;
  Assert.ok(
    !aboutNewTabService.overridden,
    "Newtab URL should not be overridden"
  );
  Assert.equal(
    aboutNewTabService.newTabURL,
    "about:newtab",
    "Newtab URL should be the default"
  );

  // test override to a chrome URL
  notificationPromise = nextChangeNotificationPromise(DOWNLOADS_URL);
  aboutNewTabService.newTabURL = DOWNLOADS_URL;
  await notificationPromise;
  Assert.ok(aboutNewTabService.overridden, "Newtab URL should be overridden");
  Assert.equal(
    aboutNewTabService.newTabURL,
    DOWNLOADS_URL,
    "Newtab URL should be the custom URL"
  );

  cleanup();
});

addTestsWithPrivilegedContentProcessPref(
  async function test_override_activity_stream_enabled() {
    Assert.equal(
      aboutNewTabService.defaultURL,
      ACTIVITY_STREAM_URL,
      "Newtab URL should be the default activity stream URL"
    );
    Assert.ok(
      !aboutNewTabService.overridden,
      "Newtab URL should not be overridden"
    );
    Assert.ok(
      aboutNewTabService.activityStreamEnabled,
      "Activity Stream should be enabled"
    );

    // change to a chrome URL while activity stream is enabled
    let notificationPromise = nextChangeNotificationPromise(DOWNLOADS_URL);
    aboutNewTabService.newTabURL = DOWNLOADS_URL;
    await notificationPromise;
    Assert.equal(
      aboutNewTabService.newTabURL,
      DOWNLOADS_URL,
      "Newtab URL set to chrome url"
    );
    Assert.equal(
      aboutNewTabService.defaultURL,
      ACTIVITY_STREAM_URL,
      "Newtab URL defaultURL still set to the default activity stream URL"
    );
    Assert.ok(aboutNewTabService.overridden, "Newtab URL should be overridden");
    Assert.ok(
      !aboutNewTabService.activityStreamEnabled,
      "Activity Stream should not be enabled"
    );

    cleanup();
  }
);

addTestsWithPrivilegedContentProcessPref(async function test_default_url() {
  Assert.equal(
    aboutNewTabService.defaultURL,
    ACTIVITY_STREAM_URL,
    "Newtab defaultURL initially set to AS url"
  );

  // Only debug variants aren't available on release/beta
  if (!IS_RELEASE_OR_BETA) {
    await setBoolPrefAndWaitForChange(
      ACTIVITY_STREAM_DEBUG_PREF,
      true,
      "A notification occurs after changing the debug pref to true"
    );
    Assert.equal(
      aboutNewTabService.activityStreamDebug,
      true,
      "the .activityStreamDebug property is set to true"
    );
    Assert.equal(
      aboutNewTabService.defaultURL,
      ACTIVITY_STREAM_DEBUG_URL,
      "Newtab defaultURL set to debug AS url after the pref has been changed"
    );
    await setBoolPrefAndWaitForChange(
      ACTIVITY_STREAM_DEBUG_PREF,
      false,
      "A notification occurs after changing the debug pref to false"
    );
  } else {
    Services.prefs.setBoolPref(ACTIVITY_STREAM_DEBUG_PREF, true);

    Assert.equal(
      aboutNewTabService.activityStreamDebug,
      false,
      "the .activityStreamDebug property is remains false"
    );
  }

  Assert.equal(
    aboutNewTabService.defaultURL,
    ACTIVITY_STREAM_URL,
    "Newtab defaultURL set to un-prerendered AS if prerender is false and debug is false"
  );

  cleanup();
});

addTestsWithPrivilegedContentProcessPref(async function test_welcome_url() {
  Assert.equal(
    aboutNewTabService.welcomeURL,
    ACTIVITY_STREAM_URL,
    "Newtab welcomeURL set to un-prerendered AS when debug disabled."
  );
  Assert.equal(
    aboutNewTabService.welcomeURL,
    aboutNewTabService.defaultURL,
    "Newtab welcomeURL is equal to defaultURL when prerendering disabled and debug disabled."
  );

  // Only debug variants aren't available on release/beta
  if (!IS_RELEASE_OR_BETA) {
    await setBoolPrefAndWaitForChange(
      ACTIVITY_STREAM_DEBUG_PREF,
      true,
      "A notification occurs after changing the debug pref to true."
    );
    Assert.equal(
      aboutNewTabService.welcomeURL,
      ACTIVITY_STREAM_DEBUG_URL,
      "Newtab welcomeURL set to un-prerendered debug AS when debug enabled"
    );
  }

  cleanup();
});

/**
 * Tests response to updates to prefs
 */
addTestsWithPrivilegedContentProcessPref(async function test_updates() {
  // Simulates a "cold-boot" situation, with some pref already set before testing a series
  // of changes.
  aboutNewTabService.resetNewTabURL(); // need to set manually because pref notifs are off
  let notificationPromise;

  // test update fires on override and reset
  let testURL = "https://example.com/";
  notificationPromise = nextChangeNotificationPromise(
    testURL,
    "a notification occurs on override"
  );
  aboutNewTabService.newTabURL = testURL;
  await notificationPromise;

  // from overridden to default
  notificationPromise = nextChangeNotificationPromise(
    "about:newtab",
    "a notification occurs on reset"
  );
  aboutNewTabService.resetNewTabURL();
  Assert.ok(
    aboutNewTabService.activityStreamEnabled,
    "Activity Stream should be enabled"
  );
  Assert.equal(
    aboutNewTabService.defaultURL,
    ACTIVITY_STREAM_URL,
    "Default URL should be the activity stream page"
  );
  await notificationPromise;

  // reset twice, only one notification for default URL
  notificationPromise = nextChangeNotificationPromise(
    "about:newtab",
    "reset occurs"
  );
  aboutNewTabService.resetNewTabURL();
  await notificationPromise;

  cleanup();
});
