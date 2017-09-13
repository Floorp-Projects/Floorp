/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

const ACTIVITY_STREAM_PRERENDER_URL = aboutNewTabService.activityStreamPrerenderURL;

const DEFAULT_CHROME_URL = "chrome://browser/content/newtab/newTab.xhtml";
const DOWNLOADS_URL = "chrome://browser/content/downloads/contentAreaDownloadsView.xul";
const ACTIVITY_STREAM_PREF = "browser.newtabpage.activity-stream.enabled";
const ACTIVITY_STREAM_PRERENDER_PREF = "browser.newtabpage.activity-stream.prerender";

function cleanup() {
  Services.prefs.clearUserPref(ACTIVITY_STREAM_PREF);
  Services.prefs.clearUserPref(ACTIVITY_STREAM_PRERENDER_PREF);
  aboutNewTabService.resetNewTabURL();
}

do_register_cleanup(cleanup);

/**
 * Test the overriding of the default URL
 */
add_task(async function test_override_activity_stream_disabled() {
  let notificationPromise;
  Services.prefs.setBoolPref(ACTIVITY_STREAM_PREF, false);

  // tests default is the local newtab resource
  Assert.equal(aboutNewTabService.defaultURL, DEFAULT_CHROME_URL,
               `Default newtab URL should be ${DEFAULT_CHROME_URL}`);

  // override with some remote URL
  let url = "http://example.com/";
  notificationPromise = nextChangeNotificationPromise(url);
  aboutNewTabService.newTabURL = url;
  await notificationPromise;
  Assert.ok(aboutNewTabService.overridden, "Newtab URL should be overridden");
  Assert.ok(!aboutNewTabService.activityStreamEnabled, "Newtab activity stream should not be enabled");
  Assert.equal(aboutNewTabService.newTabURL, url, "Newtab URL should be the custom URL");

  // test reset with activity stream disabled
  notificationPromise = nextChangeNotificationPromise("about:newtab");
  aboutNewTabService.resetNewTabURL();
  await notificationPromise;
  Assert.ok(!aboutNewTabService.overridden, "Newtab URL should not be overridden");
  Assert.equal(aboutNewTabService.newTabURL, "about:newtab", "Newtab URL should be the default");

  // test override to a chrome URL
  notificationPromise = nextChangeNotificationPromise(DOWNLOADS_URL);
  aboutNewTabService.newTabURL = DOWNLOADS_URL;
  await notificationPromise;
  Assert.ok(aboutNewTabService.overridden, "Newtab URL should be overridden");
  Assert.equal(aboutNewTabService.newTabURL, DOWNLOADS_URL, "Newtab URL should be the custom URL");

  cleanup();
});

add_task(async function test_override_activity_stream_enabled() {
  let notificationPromise = await setupASPrerendered();

  Assert.equal(aboutNewTabService.defaultURL, ACTIVITY_STREAM_PRERENDER_URL, "Newtab URL should be the default activity stream prerendered URL");
  Assert.ok(!aboutNewTabService.overridden, "Newtab URL should not be overridden");
  Assert.ok(aboutNewTabService.activityStreamEnabled, "Activity Stream should be enabled");
  Assert.ok(aboutNewTabService.activityStreamPrerender, "Activity Stream should be prerendered");

  // change to local newtab page while activity stream is enabled
  notificationPromise = nextChangeNotificationPromise(DEFAULT_CHROME_URL);
  aboutNewTabService.newTabURL = DEFAULT_CHROME_URL;
  await notificationPromise;
  Assert.equal(aboutNewTabService.newTabURL, DEFAULT_CHROME_URL,
               "Newtab URL set to chrome url");
  Assert.equal(aboutNewTabService.defaultURL, DEFAULT_CHROME_URL,
               "Newtab URL defaultURL set to the default chrome URL");
  Assert.ok(aboutNewTabService.overridden, "Newtab URL should be overridden");
  Assert.ok(!aboutNewTabService.activityStreamEnabled, "Activity Stream should not be enabled");

  cleanup();
});

add_task(async function test_default_url() {
  await setupASPrerendered();
  Assert.equal(aboutNewTabService.defaultURL, ACTIVITY_STREAM_PRERENDER_URL,
               "Newtab defaultURL initially set to prerendered AS url");

  // Change activity-stream.prerendered to false and wait for the required event to fire
  const notificationPromise = nextChangeNotificationPromise(
    "about:newtab", "a notification occurs after changing prerender pref");
  Services.prefs.setBoolPref(ACTIVITY_STREAM_PRERENDER_PREF, false);

  await notificationPromise;

  Assert.equal(aboutNewTabService.defaultURL, aboutNewTabService.activityStreamURL,
               "Newtab defaultURL set to un-prerendered AS url after the pref has been changed");

  cleanup();
});

/**
 * Tests reponse to updates to prefs
 */
add_task(async function test_updates() {
  /*
   * Simulates a "cold-boot" situation, with some pref already set before testing a series
   * of changes.
   */
  await setupASPrerendered();

  aboutNewTabService.resetNewTabURL(); // need to set manually because pref notifs are off
  let notificationPromise;

  // test update fires on override and reset
  let testURL = "https://example.com/";
  notificationPromise = nextChangeNotificationPromise(
    testURL, "a notification occurs on override");
  aboutNewTabService.newTabURL = testURL;
  await notificationPromise;

  // from overridden to default
  notificationPromise = nextChangeNotificationPromise(
    "about:newtab", "a notification occurs on reset");
  aboutNewTabService.resetNewTabURL();
  Assert.ok(aboutNewTabService.activityStreamEnabled, "Activity Stream should be enabled");
  Assert.equal(aboutNewTabService.defaultURL, ACTIVITY_STREAM_PRERENDER_URL, "Default URL should be the activity stream page");
  await notificationPromise;

  // reset twice, only one notification for default URL
  notificationPromise = nextChangeNotificationPromise(
    "about:newtab", "reset occurs");
  aboutNewTabService.resetNewTabURL();
  await notificationPromise;

  cleanup();
});

function nextChangeNotificationPromise(aNewURL, testMessage) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {  // jshint unused:false
      Services.obs.removeObserver(observer, aTopic);
      Assert.equal(aData, aNewURL, testMessage);
      resolve();
    }, "newtab-url-changed");
  });
}

function setupASPrerendered() {
  if (Services.prefs.getBoolPref(ACTIVITY_STREAM_PREF) &&
    Services.prefs.getBoolPref(ACTIVITY_STREAM_PRERENDER_PREF)) {
    return Promise.resolve();
  }

  let notificationPromise;
  // change newtab page to activity stream
  notificationPromise = nextChangeNotificationPromise("about:newtab");
  Services.prefs.setBoolPref(ACTIVITY_STREAM_PREF, true);
  Services.prefs.setBoolPref(ACTIVITY_STREAM_PRERENDER_PREF, true);
  return notificationPromise;
}
