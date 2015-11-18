/* globals ok, equal, RemoteNewTabLocation, NewTabPrefsProvider, Services, Preferences */
/* jscs:disable requireCamelCaseOrUpperCaseIdentifiers */
"use strict";

Components.utils.import("resource:///modules/RemoteNewTabLocation.jsm");
Components.utils.import("resource:///modules/NewTabPrefsProvider.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/Preferences.jsm");
Components.utils.importGlobalProperties(["URL"]);

RemoteNewTabLocation.init();
const DEFAULT_HREF = RemoteNewTabLocation.href;
RemoteNewTabLocation.uninit();

add_task(function* test_defaults() {
  RemoteNewTabLocation.init();
  ok(RemoteNewTabLocation.href, "Default location has an href");
  ok(RemoteNewTabLocation.origin, "Default location has an origin");
  ok(!RemoteNewTabLocation.overridden, "Default location is not overridden");
  RemoteNewTabLocation.uninit();
});

/**
 * Tests the overriding of the default URL
 */
add_task(function* test_overrides() {
  RemoteNewTabLocation.init();
  let testURL = new URL("https://example.com/");
  let notificationPromise;

  notificationPromise = nextChangeNotificationPromise(
    testURL.href, "Remote Location should change");
  RemoteNewTabLocation.override(testURL.href);
  yield notificationPromise;
  ok(RemoteNewTabLocation.overridden, "Remote location should be overridden");
  equal(RemoteNewTabLocation.href, testURL.href,
        "Remote href should be the custom URL");
  equal(RemoteNewTabLocation.origin, testURL.origin,
        "Remote origin should be the custom URL");

  notificationPromise = nextChangeNotificationPromise(
    DEFAULT_HREF, "Remote href should be reset");
  RemoteNewTabLocation.reset();
  yield notificationPromise;
  ok(!RemoteNewTabLocation.overridden, "Newtab URL should not be overridden");
  RemoteNewTabLocation.uninit();
});

/**
 * Tests how RemoteNewTabLocation responds to updates to prefs
 */
add_task(function* test_updates() {
  RemoteNewTabLocation.init();
  let notificationPromise;
  let expectedHref = "https://newtab.cdn.mozilla.net" +
                     `/v${RemoteNewTabLocation.version}` +
                     "/nightly" +
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
  let testURL = new URL("https://example.com/");
  notificationPromise = nextChangeNotificationPromise(
    testURL.href, "a notification occurs on override");
  RemoteNewTabLocation.override(testURL.href);
  yield notificationPromise;

  // from overridden to default
  notificationPromise = nextChangeNotificationPromise(
    DEFAULT_HREF, "a notification occurs on reset");
  RemoteNewTabLocation.reset();
  yield notificationPromise;

  // override to default URL from default URL
  notificationPromise = nextChangeNotificationPromise(
    testURL.href, "a notification only occurs for a change in overridden urls");
  RemoteNewTabLocation.override(DEFAULT_HREF);
  RemoteNewTabLocation.override(testURL.href);
  yield notificationPromise;

  // reset twice, only one notification for default URL
  notificationPromise = nextChangeNotificationPromise(
    DEFAULT_HREF, "reset occurs");
  RemoteNewTabLocation.reset();
  yield notificationPromise;

  notificationPromise = nextChangeNotificationPromise(
    testURL.href, "a notification only occurs for a change in reset urls");
  RemoteNewTabLocation.reset();
  RemoteNewTabLocation.override(testURL.href);
  yield notificationPromise;

  NewTabPrefsProvider.prefs.uninit();
  RemoteNewTabLocation.uninit();
});

/**
 * Verifies that RemoteNewTabLocation's _releaseFromUpdateChannel
 * Returns the correct release names
 */
add_task(function* test_release_names() {
  RemoteNewTabLocation.init();
  let valid_channels = RemoteNewTabLocation.channels;
  let invalid_channels = new Set(["default", "invalid"]);

  for (let channel of valid_channels) {
    equal(channel, RemoteNewTabLocation._releaseFromUpdateChannel(channel),
          "release == channel name when valid");
  }

  for (let channel of invalid_channels) {
    equal("nightly", RemoteNewTabLocation._releaseFromUpdateChannel(channel),
          "release == nightly when invalid");
  }
  RemoteNewTabLocation.uninit();
});

function nextChangeNotificationPromise(aNewURL, testMessage) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) { // jshint ignore:line
      Services.obs.removeObserver(observer, aTopic);
      equal(aData, aNewURL, testMessage);
      resolve();
    }, "remote-new-tab-location-changed", false);
  });
}
