/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that sites added to the Tracking Protection whitelist in private
// browsing mode don't persist once the private browsing window closes.

const PB_PREF = "privacy.trackingprotection.pbmode.enabled";
const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/trackingPage.html";
let TrackingProtection = null;
let browser = null;
let {UrlClassifierTestUtils} = Cu.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

registerCleanupFunction(function() {
  TrackingProtection = browser = null;
  UrlClassifierTestUtils.cleanupTestTrackers();
});

function hidden(sel) {
  let win = browser.ownerGlobal;
  let el = win.document.querySelector(sel);
  let display = win.getComputedStyle(el).getPropertyValue("display", null);
  return display === "none";
}

function identityPopupState() {
  let win = browser.ownerGlobal;
  return win.document.getElementById("identity-popup").state;
}

function clickButton(sel) {
  let win = browser.ownerGlobal;
  let el = win.document.querySelector(sel);
  el.doCommand();
}

function testTrackingPage(window) {
  info("Tracking content must be blocked");
  ok(!TrackingProtection.container.hidden, "The container is visible");
  is(TrackingProtection.content.getAttribute("state"), "blocked-tracking-content",
     'content: state="blocked-tracking-content"');
  is(TrackingProtection.icon.getAttribute("state"), "blocked-tracking-content",
     'icon: state="blocked-tracking-content"');

  ok(!hidden("#tracking-protection-icon"), "icon is visible");
  ok(hidden("#tracking-action-block"), "blockButton is hidden");

  ok(hidden("#tracking-action-unblock"), "unblockButton is hidden");
  ok(!hidden("#tracking-action-unblock-private"), "unblockButtonPrivate is visible");

  // Make sure that the blocked tracking elements message appears
  ok(hidden("#tracking-not-detected"), "labelNoTracking is hidden");
  ok(hidden("#tracking-loaded"), "labelTrackingLoaded is hidden");
  ok(!hidden("#tracking-blocked"), "labelTrackingBlocked is visible");
}

function testTrackingPageUnblocked() {
  info("Tracking content must be white-listed and not blocked");
  ok(!TrackingProtection.container.hidden, "The container is visible");
  is(TrackingProtection.content.getAttribute("state"), "loaded-tracking-content",
     'content: state="loaded-tracking-content"');
  is(TrackingProtection.icon.getAttribute("state"), "loaded-tracking-content",
     'icon: state="loaded-tracking-content"');

  ok(!hidden("#tracking-protection-icon"), "icon is visible");
  ok(!hidden("#tracking-action-block"), "blockButton is visible");
  ok(hidden("#tracking-action-unblock"), "unblockButton is hidden");

  // Make sure that the blocked tracking elements message appears
  ok(hidden("#tracking-not-detected"), "labelNoTracking is hidden");
  ok(!hidden("#tracking-loaded"), "labelTrackingLoaded is visible");
  ok(hidden("#tracking-blocked"), "labelTrackingBlocked is hidden");
}

add_task(function* testExceptionAddition() {
  yield UrlClassifierTestUtils.addTestTrackers();
  let privateWin = yield promiseOpenAndLoadWindow({private: true}, true);
  browser = privateWin.gBrowser;
  let tab = browser.selectedTab = browser.addTab();

  TrackingProtection = browser.ownerGlobal.TrackingProtection;
  yield pushPrefs([PB_PREF, true]);

  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");

  info("Load a test page containing tracking elements");
  yield promiseTabLoadEvent(tab, TRACKING_PAGE);

  testTrackingPage(tab.ownerDocument.defaultView);

  info("Disable TP for the page (which reloads the page)");
  let tabReloadPromise = promiseTabLoadEvent(tab);
  clickButton("#tracking-action-unblock");
  is(identityPopupState(), "closed", "foobar");

  yield tabReloadPromise;
  testTrackingPageUnblocked();

  info("Test that the exception is remembered across tabs in the same private window");
  tab = browser.selectedTab = browser.addTab();

  info("Load a test page containing tracking elements");
  yield promiseTabLoadEvent(tab, TRACKING_PAGE);
  testTrackingPageUnblocked();

  yield promiseWindowClosed(privateWin);
});

add_task(function* testExceptionPersistence() {
  info("Open another private browsing window");
  let privateWin = yield promiseOpenAndLoadWindow({private: true}, true);
  browser = privateWin.gBrowser;
  let tab = browser.selectedTab = browser.addTab();

  TrackingProtection = browser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection.enabled, "TP is still enabled");

  info("Load a test page containing tracking elements");
  yield promiseTabLoadEvent(tab, TRACKING_PAGE);

  testTrackingPage(tab.ownerDocument.defaultView);

  info("Disable TP for the page (which reloads the page)");
  let tabReloadPromise = promiseTabLoadEvent(tab);
  clickButton("#tracking-action-unblock");
  is(identityPopupState(), "closed", "foobar");

  yield tabReloadPromise;
  testTrackingPageUnblocked();

  privateWin.close();
});
