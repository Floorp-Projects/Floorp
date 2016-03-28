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

XPCOMUtils.defineLazyModuleGetter(this, "Locale",
                                  "resource://gre/modules/Locale.jsm");

const DEFAULT_HREF = aboutNewTabService.generateRemoteURL();
const DEFAULT_CHROME_URL = "chrome://browser/content/newtab/newTab.xhtml";
const DOWNLOADS_URL = "chrome://browser/content/downloads/contentAreaDownloadsView.xul";
const DEFAULT_VERSION = aboutNewTabService.remoteVersion;

function cleanup() {
  Services.prefs.setBoolPref("browser.newtabpage.remote", false);
  Services.prefs.setCharPref("browser.newtabpage.remote.version", DEFAULT_VERSION);
  aboutNewTabService.resetNewTabURL();
  NewTabPrefsProvider.prefs.uninit();
}

do_register_cleanup(cleanup);

/**
 * Test the overriding of the default URL
 */
add_task(function* test_override_remote_disabled() {
  NewTabPrefsProvider.prefs.init();
  let notificationPromise;
  Services.prefs.setBoolPref("browser.newtabpage.remote", false);

  // tests default is the local newtab resource
  Assert.equal(aboutNewTabService.defaultURL, DEFAULT_CHROME_URL,
               `Default newtab URL should be ${DEFAULT_CHROME_URL}`);

  // override with some remote URL
  let url = "http://example.com/";
  notificationPromise = nextChangeNotificationPromise(url);
  aboutNewTabService.newTabURL = url;
  yield notificationPromise;
  Assert.ok(aboutNewTabService.overridden, "Newtab URL should be overridden");
  Assert.ok(!aboutNewTabService.remoteEnabled, "Newtab remote should not be enabled");
  Assert.equal(aboutNewTabService.newTabURL, url, "Newtab URL should be the custom URL");

  // test reset with remote disabled
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

add_task(function* test_override_remote_enabled() {
  NewTabPrefsProvider.prefs.init();
  let notificationPromise;
  // change newtab page to remote
  notificationPromise = nextChangeNotificationPromise("about:newtab");
  Services.prefs.setBoolPref("browser.newtabpage.remote", true);
  yield notificationPromise;
  let remoteHref = aboutNewTabService.generateRemoteURL();
  Assert.equal(aboutNewTabService.defaultURL, remoteHref, "Newtab URL should be the default remote URL");
  Assert.ok(!aboutNewTabService.overridden, "Newtab URL should not be overridden");
  Assert.ok(aboutNewTabService.remoteEnabled, "Newtab remote should be enabled");

  // change to local newtab page while remote is enabled
  notificationPromise = nextChangeNotificationPromise(DEFAULT_CHROME_URL);
  aboutNewTabService.newTabURL = DEFAULT_CHROME_URL;
  yield notificationPromise;
  Assert.equal(aboutNewTabService.newTabURL, DEFAULT_CHROME_URL,
               "Newtab URL set to chrome url");
  Assert.equal(aboutNewTabService.defaultURL, DEFAULT_CHROME_URL,
               "Newtab URL defaultURL set to the default chrome URL");
  Assert.ok(aboutNewTabService.overridden, "Newtab URL should be overridden");
  Assert.ok(!aboutNewTabService.remoteEnabled, "Newtab remote should not be enabled");

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
  Preferences.set("browser.newtabpage.remote", true);
  aboutNewTabService.resetNewTabURL(); // need to set manually because pref notifs are off
  let notificationPromise;
  let productionModeBaseUrl = "https://content.cdn.mozilla.net";
  let testModeBaseUrl = "https://example.com";
  let expectedPath = `/newtab` +
                     `/v${aboutNewTabService.remoteVersion}` +
                     `/${aboutNewTabService.remoteReleaseName}` +
                     "/en-GB" +
                     "/index.html";
  let expectedHref = productionModeBaseUrl + expectedPath;
  Preferences.set("intl.locale.matchOS", true);
  Preferences.set("general.useragent.locale", "en-GB");
  Preferences.set("browser.newtabpage.remote.mode", "production");
  NewTabPrefsProvider.prefs.init();

  // test update checks for prefs
  notificationPromise = nextChangeNotificationPromise(
    expectedHref, "Remote href should be updated");
  Preferences.set("intl.locale.matchOS", false);
  yield notificationPromise;

  notificationPromise = nextChangeNotificationPromise(
    DEFAULT_HREF, "Remote href changes back to default");
  Preferences.set("general.useragent.locale", "en-US");
  yield notificationPromise;

  // test update fires when mode is changed
  expectedPath = expectedPath.replace("/en-GB/", "/en-US/");
  notificationPromise = nextChangeNotificationPromise(
    testModeBaseUrl + expectedPath, "Remote href changes back to origin of test mode");
  Preferences.set("browser.newtabpage.remote.mode", "test");
  yield notificationPromise;

  // test invalid mode ends up pointing to production url
  notificationPromise = nextChangeNotificationPromise(
    DEFAULT_HREF, "Remote href changes back to production default");
  Preferences.set("browser.newtabpage.remote.mode", "invalid");
  yield notificationPromise;

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
  Assert.ok(aboutNewTabService.remoteEnabled, "Newtab remote should be enabled");
  Assert.equal(aboutNewTabService.defaultURL, DEFAULT_HREF, "Default URL should be the remote page");
  yield notificationPromise;

  // override to default URL from default URL
  notificationPromise = nextChangeNotificationPromise(
    testURL, "a notification only occurs for a change in overridden urls");
  aboutNewTabService.newTabURL = aboutNewTabService.generateRemoteURL();
  Assert.ok(aboutNewTabService.remoteEnabled, "Newtab remote should be enabled");
  aboutNewTabService.newTabURL = testURL;
  yield notificationPromise;
  Assert.ok(!aboutNewTabService.remoteEnabled, "Newtab remote should not be enabled");

  // reset twice, only one notification for default URL
  notificationPromise = nextChangeNotificationPromise(
    "about:newtab", "reset occurs");
  aboutNewTabService.resetNewTabURL();
  yield notificationPromise;

  cleanup();
});

/**
 * Verifies that releaseFromUpdateChannel
 * Returns the correct release names
 */
add_task(function* test_release_names() {
  let valid_channels = ["esr", "release", "beta", "aurora", "nightly"];
  let invalid_channels = new Set(["default", "invalid"]);

  for (let channel of valid_channels) {
    Assert.equal(channel, aboutNewTabService.releaseFromUpdateChannel(channel),
          "release == channel name when valid");
  }

  for (let channel of invalid_channels) {
    Assert.equal("nightly", aboutNewTabService.releaseFromUpdateChannel(channel),
          "release == nightly when invalid");
  }
});

/**
 * Verifies that remote version updates changes the remote newtab url
 */
add_task(function* test_version_update() {
  NewTabPrefsProvider.prefs.init();

  Services.prefs.setBoolPref("browser.newtabpage.remote", true);
  Assert.ok(aboutNewTabService.remoteEnabled, "remote mode enabled");

  let productionModeBaseUrl = "https://content.cdn.mozilla.net";
  let version_incr = String(parseInt(DEFAULT_VERSION) + 1);
  let expectedPath = `/newtab` +
                     `/v${version_incr}` +
                     `/${aboutNewTabService.remoteReleaseName}` +
                     `/${Locale.getLocale()}` +
                     `/index.html`;
  let expectedHref = productionModeBaseUrl + expectedPath;

  let notificationPromise;
  notificationPromise = nextChangeNotificationPromise(expectedHref);
  Preferences.set("browser.newtabpage.remote.version", version_incr);
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
