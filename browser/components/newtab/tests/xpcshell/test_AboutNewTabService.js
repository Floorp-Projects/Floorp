/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* globals Services, XPCOMUtils, NewTabPrefsProvider, Preferences, aboutNewTabService, do_register_cleanup */

"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NewTabPrefsProvider",
                                  "resource:///modules/NewTabPrefsProvider.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

const DEFAULT_HREF = aboutNewTabService.activityStreamURL;
const DEFAULT_CHROME_URL = "chrome://browser/content/newtab/newTab.xhtml";
const DOWNLOADS_URL = "chrome://browser/content/downloads/contentAreaDownloadsView.xul";

function cleanup() {
  Services.prefs.setBoolPref("browser.newtabpage.activity-stream.enabled", false);
  aboutNewTabService.resetNewTabURL();
  NewTabPrefsProvider.prefs.uninit();
}

do_register_cleanup(cleanup);

/**
 * Test the overriding of the default URL
 */
add_task(function* test_override_activity_stream_disabled() {
  NewTabPrefsProvider.prefs.init();
  let notificationPromise;
  Services.prefs.setBoolPref("browser.newtabpage.activity-stream.enabled", false);

  // tests default is the local newtab resource
  Assert.equal(aboutNewTabService.defaultURL, DEFAULT_CHROME_URL,
               `Default newtab URL should be ${DEFAULT_CHROME_URL}`);

  // override with some remote URL
  let url = "http://example.com/";
  notificationPromise = nextChangeNotificationPromise(url);
  aboutNewTabService.newTabURL = url;
  yield notificationPromise;
  Assert.ok(aboutNewTabService.overridden, "Newtab URL should be overridden");
  Assert.ok(!aboutNewTabService.activityStreamEnabled, "Newtab activity stream should not be enabled");
  Assert.equal(aboutNewTabService.newTabURL, url, "Newtab URL should be the custom URL");

  // test reset with activity stream disabled
  notificationPromise = nextChangeNotificationPromise("about:newtab");
  aboutNewTabService.resetNewTabURL();
  yield notificationPromise;
  Assert.ok(!aboutNewTabService.overridden, "Newtab URL should not be overridden");
  Assert.equal(aboutNewTabService.newTabURL, "about:newtab", "Newtab URL should be the default");

  // test override to a chrome URL
  notificationPromise = nextChangeNotificationPromise(DOWNLOADS_URL);
  aboutNewTabService.newTabURL = DOWNLOADS_URL;
  yield notificationPromise;
  Assert.ok(aboutNewTabService.overridden, "Newtab URL should be overridden");
  Assert.equal(aboutNewTabService.newTabURL, DOWNLOADS_URL, "Newtab URL should be the custom URL");

  cleanup();
});

add_task(function* test_override_activity_stream_enabled() {
  NewTabPrefsProvider.prefs.init();
  let notificationPromise;
  // change newtab page to activity stream
  notificationPromise = nextChangeNotificationPromise("about:newtab");
  Services.prefs.setBoolPref("browser.newtabpage.activity-stream.enabled", true);
  yield notificationPromise;
  let activityStreamURL = aboutNewTabService.activityStreamURL;
  Assert.equal(aboutNewTabService.defaultURL, activityStreamURL, "Newtab URL should be the default activity stream URL");
  Assert.ok(!aboutNewTabService.overridden, "Newtab URL should not be overridden");
  Assert.ok(aboutNewTabService.activityStreamEnabled, "Activity Stream should be enabled");

  // change to local newtab page while activity stream is enabled
  notificationPromise = nextChangeNotificationPromise(DEFAULT_CHROME_URL);
  aboutNewTabService.newTabURL = DEFAULT_CHROME_URL;
  yield notificationPromise;
  Assert.equal(aboutNewTabService.newTabURL, DEFAULT_CHROME_URL,
               "Newtab URL set to chrome url");
  Assert.equal(aboutNewTabService.defaultURL, DEFAULT_CHROME_URL,
               "Newtab URL defaultURL set to the default chrome URL");
  Assert.ok(aboutNewTabService.overridden, "Newtab URL should be overridden");
  Assert.ok(!aboutNewTabService.activityStreamEnabled, "Activity Stream should not be enabled");

  cleanup();
});

/**
 * Tests reponse to updates to prefs
 */
add_task(function* test_updates() {
  /*
   * Simulates a "cold-boot" situation, with some pref already set before testing a series
   * of changes.
   */
  Preferences.set("browser.newtabpage.activity-stream.enabled", true);
  aboutNewTabService.resetNewTabURL(); // need to set manually because pref notifs are off
  let notificationPromise;
  NewTabPrefsProvider.prefs.init();

  // test update fires on override and reset
  let testURL = "https://example.com/";
  notificationPromise = nextChangeNotificationPromise(
    testURL, "a notification occurs on override");
  aboutNewTabService.newTabURL = testURL;
  yield notificationPromise;

  // from overridden to default
  notificationPromise = nextChangeNotificationPromise(
    "about:newtab", "a notification occurs on reset");
  aboutNewTabService.resetNewTabURL();
  Assert.ok(aboutNewTabService.activityStreamEnabled, "Activity Stream should be enabled");
  Assert.equal(aboutNewTabService.defaultURL, DEFAULT_HREF, "Default URL should be the activity stream page");
  yield notificationPromise;

  // reset twice, only one notification for default URL
  notificationPromise = nextChangeNotificationPromise(
    "about:newtab", "reset occurs");
  aboutNewTabService.resetNewTabURL();
  yield notificationPromise;

  cleanup();
});

function nextChangeNotificationPromise(aNewURL, testMessage) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {  // jshint unused:false
      Services.obs.removeObserver(observer, aTopic);
      Assert.equal(aData, aNewURL, testMessage);
      resolve();
    }, "newtab-url-changed", false);
  });
}
