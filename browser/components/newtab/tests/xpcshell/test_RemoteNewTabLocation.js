/* globals ok, equal, RemoteNewTabLocation, Services */
"use strict";

Components.utils.import("resource:///modules/RemoteNewTabLocation.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.importGlobalProperties(["URL"]);

add_task(function* () {
  var notificationPromise;
  let defaultHref = RemoteNewTabLocation.href;

  ok(RemoteNewTabLocation.href, "Default location has an href");
  ok(RemoteNewTabLocation.origin, "Default location has an origin");
  ok(!RemoteNewTabLocation.overridden, "Default location is not overridden");

  let testURL = new URL("https://example.com/");

  notificationPromise = changeNotificationPromise(testURL.href);
  RemoteNewTabLocation.override(testURL.href);
  yield notificationPromise;
  ok(RemoteNewTabLocation.overridden, "Remote location should be overridden");
  equal(RemoteNewTabLocation.href, testURL.href, "Remote href should be the custom URL");
  equal(RemoteNewTabLocation.origin, testURL.origin, "Remote origin should be the custom URL");

  notificationPromise = changeNotificationPromise(defaultHref);
  RemoteNewTabLocation.reset();
  yield notificationPromise;
  ok(!RemoteNewTabLocation.overridden, "Newtab URL should not be overridden");
  equal(RemoteNewTabLocation.href, defaultHref, "Remote href should be reset");
});

function changeNotificationPromise(aNewURL) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) { // jshint ignore:line
      Services.obs.removeObserver(observer, aTopic);
      equal(aData, aNewURL, "remote-new-tab-location-changed data should be new URL.");
      resolve();
    }, "remote-new-tab-location-changed", false);
  });
}
