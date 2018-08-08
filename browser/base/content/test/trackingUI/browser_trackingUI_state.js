/*
 * Test that the Tracking Protection section is visible in the Control Center
 * and has the correct state for the cases when:
 *
 * In a normal window as well as a private window,
 *   With TP enabled
 *     1) A page with no tracking elements is loaded.
 *     2) A page with tracking elements is loaded and they are blocked.
 *     3) A page with tracking elements is loaded and they are not blocked.
 *   With TP disabled
 *     1) A page with no tracking elements is loaded.
 *     2) A page with tracking elements is loaded.
 *
 * See also Bugs 1175327, 1043801, 1178985
 */

const PREF = "privacy.trackingprotection.enabled";
const PB_PREF = "privacy.trackingprotection.pbmode.enabled";
const BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/benignPage.html";
const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";
var TrackingProtection = null;
var tabbrowser = null;

registerCleanupFunction(function() {
  TrackingProtection = tabbrowser = null;
  UrlClassifierTestUtils.cleanupTestTrackers();
  Services.prefs.clearUserPref(PREF);
  Services.prefs.clearUserPref(PB_PREF);
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

// This is a special version of "hidden" that doesn't check for item
// visibility and just asserts the display and opacity attributes.
// That way we can test elements even when their panel is hidden...
function hidden(sel) {
  let win = tabbrowser.ownerGlobal;
  let el = win.document.querySelector(sel);
  let display = win.getComputedStyle(el).getPropertyValue("display", null);
  let opacity = win.getComputedStyle(el).getPropertyValue("opacity", null);
  return display === "none" || opacity === "0";
}

function clickButton(sel) {
  let win = tabbrowser.ownerGlobal;
  let el = win.document.querySelector(sel);
  el.doCommand();
}

function testBenignPage() {
  info("Non-tracking content must not be blocked");
  ok(!TrackingProtection.container.hidden, "The container is visible");
  ok(!TrackingProtection.content.hasAttribute("state"), "content: no state");
  ok(!TrackingProtection.iconBox.hasAttribute("state"), "icon box: no state");
  ok(!TrackingProtection.iconBox.hasAttribute("tooltiptext"), "icon box: no tooltip");

  let doc = tabbrowser.ownerGlobal.document;
  ok(BrowserTestUtils.is_hidden(doc.getElementById("tracking-protection-icon-box")), "icon box is hidden");
  ok(hidden("#tracking-action-block"), "blockButton is hidden");
  ok(hidden("#tracking-action-unblock"), "unblockButton is hidden");
  ok(!hidden("#tracking-protection-preferences-button"), "preferences button is visible");

  // Make sure that the no tracking elements message appears
  ok(!hidden("#tracking-not-detected"), "labelNoTracking is visible");
  ok(hidden("#tracking-loaded"), "labelTrackingLoaded is hidden");
  ok(hidden("#tracking-blocked"), "labelTrackingBlocked is hidden");
}

function testBenignPageWithException() {
  info("Non-tracking content must not be blocked");
  ok(!TrackingProtection.container.hidden, "The container is visible");
  ok(!TrackingProtection.content.hasAttribute("state"), "content: no state");
  ok(TrackingProtection.content.hasAttribute("hasException"), "content has exception attribute");
  ok(TrackingProtection.iconBox.hasAttribute("hasException"), "icon box has exception attribute");
  ok(!TrackingProtection.iconBox.hasAttribute("state"), "icon box: no state");
  ok(!TrackingProtection.iconBox.hasAttribute("tooltiptext"), "icon box: no tooltip");

  let doc = tabbrowser.ownerGlobal.document;
  ok(BrowserTestUtils.is_hidden(doc.getElementById("tracking-protection-icon-box")), "icon box is hidden");
  is(!hidden("#tracking-action-block"), TrackingProtection.enabled,
     "blockButton is visible if TP is on");
  ok(hidden("#tracking-action-unblock"), "unblockButton is hidden");
  ok(!hidden("#tracking-protection-preferences-button"), "preferences button is visible");

  is(!hidden("#tracking-not-detected-exception"), TrackingProtection.enabled,
     "labelNoTrackingException is visible if TP is on");
  is(hidden("#tracking-not-detected"), TrackingProtection.enabled,
     "labelNoTracking is visible if TP is off");
  ok(hidden("#tracking-loaded"), "labelTrackingLoaded is hidden");
  ok(hidden("#tracking-blocked"), "labelTrackingBlocked is hidden");
}

function testTrackingPage(window) {
  info("Tracking content must be blocked");
  ok(!TrackingProtection.container.hidden, "The container is visible");
  is(TrackingProtection.content.getAttribute("state"), "blocked-tracking-content",
      'content: state="blocked-tracking-content"');
  is(TrackingProtection.iconBox.getAttribute("state"), "blocked-tracking-content",
      'icon box: state="blocked-tracking-content"');
  is(TrackingProtection.iconBox.getAttribute("tooltiptext"),
     gNavigatorBundle.getString("trackingProtection.icon.activeTooltip"), "correct tooltip");
  ok(!TrackingProtection.content.hasAttribute("hasException"), "content has no exception attribute");
  ok(!TrackingProtection.iconBox.hasAttribute("hasException"), "icon box has no exception attribute");

  let doc = tabbrowser.ownerGlobal.document;
  ok(BrowserTestUtils.is_visible(doc.getElementById("tracking-protection-icon-box")), "icon box is visible");
  ok(hidden("#tracking-action-block"), "blockButton is hidden");
  ok(!hidden("#tracking-protection-preferences-button"), "preferences button is visible");


  if (PrivateBrowsingUtils.isWindowPrivate(window)) {
    ok(hidden("#tracking-action-unblock"), "unblockButton is hidden");
    ok(!hidden("#tracking-action-unblock-private"), "unblockButtonPrivate is visible");
  } else {
    ok(!hidden("#tracking-action-unblock"), "unblockButton is visible");
    ok(hidden("#tracking-action-unblock-private"), "unblockButtonPrivate is hidden");
  }

  // Make sure that the blocked tracking elements message appears
  ok(hidden("#tracking-not-detected"), "labelNoTracking is hidden");
  ok(hidden("#tracking-loaded"), "labelTrackingLoaded is hidden");
  ok(hidden("#tracking-loaded-exception"), "labelTrackingLoadedException is hidden");
  ok(!hidden("#tracking-blocked"), "labelTrackingBlocked is visible");
}

function testTrackingPageUnblocked() {
  info("Tracking content must be white-listed and not blocked");
  is(TrackingProtection.content.hasAttribute("hasException"), TrackingProtection.enabled,
    "content has exception attribute if TP is on");
  is(TrackingProtection.iconBox.hasAttribute("hasException"), TrackingProtection.enabled,
    "icon box has exception attribute if TP is on");
  ok(!TrackingProtection.container.hidden, "The container is visible");
  is(TrackingProtection.content.getAttribute("state"), "loaded-tracking-content",
      'content: state="loaded-tracking-content"');
  if (TrackingProtection.enabled) {
    is(TrackingProtection.iconBox.getAttribute("state"), "loaded-tracking-content",
        'icon box: state="loaded-tracking-content"');
    is(TrackingProtection.iconBox.getAttribute("tooltiptext"),
       gNavigatorBundle.getString("trackingProtection.icon.disabledTooltip"), "correct tooltip");
  }

  let doc = tabbrowser.ownerGlobal.document;
  is(BrowserTestUtils.is_visible(doc.getElementById("tracking-protection-icon-box")), TrackingProtection.enabled, "icon box is visible if TP is on");
  is(!hidden("#tracking-action-block"), TrackingProtection.enabled, "blockButton is visible if TP is on");
  ok(hidden("#tracking-action-unblock"), "unblockButton is hidden");
  ok(!hidden("#tracking-protection-preferences-button"), "preferences button is visible");

  // Make sure that the blocked tracking elements message appears
  ok(hidden("#tracking-not-detected"), "labelNoTracking is hidden");
  is(hidden("#tracking-loaded"), TrackingProtection.enabled,
     "labelTrackingLoaded is visible if TP is off");
  is(!hidden("#tracking-loaded-exception"), TrackingProtection.enabled,
     "labelTrackingLoadedException is visible if TP is on");
  ok(hidden("#tracking-blocked"), "labelTrackingBlocked is hidden");
}

async function testTrackingProtectionEnabled(tab) {
  info("Testing with Tracking Protection ENABLED.");

  info("Load a test page not containing tracking elements");
  await promiseTabLoadEvent(tab, BENIGN_PAGE);
  testBenignPage();

  info("Load a test page not containing tracking elements which has an exception.");
  let isPrivateBrowsing = PrivateBrowsingUtils.isWindowPrivate(tab.ownerGlobal);
  let uri = Services.io.newURI("https://example.org/");
  if (isPrivateBrowsing) {
    PrivateBrowsingUtils.addToTrackingAllowlist(uri);
  } else {
    Services.perms.add(uri, "trackingprotection", Services.perms.ALLOW_ACTION);
  }

  await promiseTabLoadEvent(tab, uri.spec);
  testBenignPageWithException();

  if (isPrivateBrowsing) {
    PrivateBrowsingUtils.removeFromTrackingAllowlist(uri);
  } else {
    Services.perms.remove(uri, "trackingprotection");
  }

  info("Load a test page containing tracking elements");
  await promiseTabLoadEvent(tab, TRACKING_PAGE);
  testTrackingPage(tab.ownerGlobal);

  info("Disable TP for the page (which reloads the page)");
  let tabReloadPromise = promiseTabLoadEvent(tab);
  clickButton("#tracking-action-unblock");
  await tabReloadPromise;
  testTrackingPageUnblocked();

  info("Re-enable TP for the page (which reloads the page)");
  tabReloadPromise = promiseTabLoadEvent(tab);
  clickButton("#tracking-action-block");
  await tabReloadPromise;
  testTrackingPage(tab.ownerGlobal);
}

async function testTrackingProtectionDisabled(tab) {
  info("Testing with Tracking Protection DISABLED.");

  info("Load a test page not containing tracking elements");
  await promiseTabLoadEvent(tab, BENIGN_PAGE);
  testBenignPage();

  info("Load a test page not containing tracking elements which has an exception.");
  let isPrivateBrowsing = PrivateBrowsingUtils.isWindowPrivate(tab.ownerGlobal);
  let uri = Services.io.newURI("https://example.org/");
  if (isPrivateBrowsing) {
    PrivateBrowsingUtils.addToTrackingAllowlist(uri);
  } else {
    Services.perms.add(uri, "trackingprotection", Services.perms.ALLOW_ACTION);
  }

  await promiseTabLoadEvent(tab, uri.spec);
  testBenignPageWithException();

  if (isPrivateBrowsing) {
    PrivateBrowsingUtils.removeFromTrackingAllowlist(uri);
  } else {
    Services.perms.remove(uri, "trackingprotection");
  }

  info("Load a test page containing tracking elements");
  await promiseTabLoadEvent(tab, TRACKING_PAGE);
  testTrackingPageUnblocked();
}

add_task(async function testNormalBrowsing() {
  await UrlClassifierTestUtils.addTestTrackers();

  tabbrowser = gBrowser;
  let tab = tabbrowser.selectedTab = tabbrowser.addTab();

  TrackingProtection = gBrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the browser window");
  is(TrackingProtection.enabled, Services.prefs.getBoolPref(PREF),
     "TP.enabled is based on the original pref value");

  Services.prefs.setBoolPref(PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");

  await testTrackingProtectionEnabled(tab);

  Services.prefs.setBoolPref(PREF, false);
  ok(!TrackingProtection.enabled, "TP is disabled after setting the pref");

  await testTrackingProtectionDisabled(tab);
});

add_task(async function testPrivateBrowsing() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  tabbrowser = privateWin.gBrowser;
  let tab = tabbrowser.selectedTab = tabbrowser.addTab();

  TrackingProtection = tabbrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the private window");
  is(TrackingProtection.enabled, Services.prefs.getBoolPref(PB_PREF),
     "TP.enabled is based on the pb pref value");

  Services.prefs.setBoolPref(PB_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");

  await testTrackingProtectionEnabled(tab);

  Services.prefs.setBoolPref(PB_PREF, false);
  ok(!TrackingProtection.enabled, "TP is disabled after setting the pref");

  await testTrackingProtectionDisabled(tab);

  privateWin.close();
});
