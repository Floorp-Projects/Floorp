/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* globals Services, XPCOMUtils, NewTabPrefsProvider, Preferences, aboutNewTabService */

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

const DEFAULT_HREF = aboutNewTabService.generateRemoteURL();

/**
 * Test the overriding of the default URL
 */
add_task(function* () {
  NewTabPrefsProvider.prefs.init();
  let notificationPromise;
  Services.prefs.setBoolPref("browser.newtabpage.remote", false);

  // tests default is the local newtab resource
  Assert.equal(aboutNewTabService.newTabURL, "chrome://browser/content/newtab/newTab.xhtml",
               "Default newtab URL should be chrome://browser/content/newtab/newTab.xhtml");

  let url = "http://example.com/";
  notificationPromise = nextChangeNotificationPromise(url);
  aboutNewTabService.newTabURL = url;
  yield notificationPromise;
  Assert.ok(aboutNewTabService.overridden, "Newtab URL should be overridden");
  Assert.ok(!aboutNewTabService.remoteEnabled, "Newtab remote should not be enabled");
  Assert.equal(aboutNewTabService.newTabURL, url, "Newtab URL should be the custom URL");

  notificationPromise = nextChangeNotificationPromise("chrome://browser/content/newtab/newTab.xhtml");
  aboutNewTabService.resetNewTabURL();
  yield notificationPromise;
  Assert.ok(!aboutNewTabService.overridden, "Newtab URL should not be overridden");
  Assert.equal(aboutNewTabService.newTabURL, "chrome://browser/content/newtab/newTab.xhtml",
               "Newtab URL should be the default");

  // change newtab page to remote
  Services.prefs.setBoolPref("browser.newtabpage.remote", true);
  let remoteHref = aboutNewTabService.generateRemoteURL();
  Assert.equal(aboutNewTabService.newTabURL, remoteHref, "Newtab URL should be the default remote URL");
  Assert.ok(!aboutNewTabService.overridden, "Newtab URL should not be overridden");
  Assert.ok(aboutNewTabService.remoteEnabled, "Newtab remote should be enabled");
  NewTabPrefsProvider.prefs.uninit();
});

/**
 * Tests reponse to updates to prefs
 */
add_task(function* test_updates() {
  Preferences.set("browser.newtabpage.remote", true);
  let notificationPromise;
  let expectedHref = "https://newtab.cdn.mozilla.net" +
                     `/v${aboutNewTabService.remoteVersion}` +
                     `/${aboutNewTabService.remoteReleaseName}` +
                     "/en-GB" +
                     "/index.html";
  Preferences.set("intl.locale.matchOS", true);
  Preferences.set("general.useragent.locale", "en-GB");
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

  // test update fires on override and reset
  let testURL = "https://example.com/";
  notificationPromise = nextChangeNotificationPromise(
    testURL, "a notification occurs on override");
  aboutNewTabService.newTabURL = testURL;
  yield notificationPromise;

  // from overridden to default
  notificationPromise = nextChangeNotificationPromise(
    DEFAULT_HREF, "a notification occurs on reset");
  aboutNewTabService.resetNewTabURL();
  Assert.ok(aboutNewTabService.remoteEnabled, "Newtab remote should be enabled");
  yield notificationPromise;

  // override to default URL from default URL
  notificationPromise = nextChangeNotificationPromise(
    testURL, "a notification only occurs for a change in overridden urls");
  aboutNewTabService.newTabURL = aboutNewTabService.generateRemoteURL();
  Assert.ok(aboutNewTabService.remoteEnabled, "Newtab remote should be enabled");
  aboutNewTabService.newTabURL = testURL;
  Assert.ok(!aboutNewTabService.remoteEnabled, "Newtab remote should not be enabled");
  yield notificationPromise;

  // reset twice, only one notification for default URL
  notificationPromise = nextChangeNotificationPromise(
    DEFAULT_HREF, "reset occurs");
  aboutNewTabService.resetNewTabURL();
  yield notificationPromise;

  NewTabPrefsProvider.prefs.uninit();
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

function nextChangeNotificationPromise(aNewURL, testMessage) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {  // jshint unused:false
      Services.obs.removeObserver(observer, aTopic);
      Assert.equal(aData, aNewURL, testMessage);
      resolve();
    }, "newtab-url-changed", false);
  });
}
