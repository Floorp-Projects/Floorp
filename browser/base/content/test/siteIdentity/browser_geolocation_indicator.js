/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource:///modules/PermissionUI.jsm", this);
ChromeUtils.import("resource:///modules/SitePermissions.jsm", this);

const CP = Cc["@mozilla.org/content-pref/service;1"].getService(
  Ci.nsIContentPrefService2
);

const EXAMPLE_PAGE_URL = "https://example.com";
const EXAMPLE_PAGE_URI = Services.io.newURI(EXAMPLE_PAGE_URL);
const GEO_CONTENT_PREF_KEY = "permissions.geoLocation.lastAccess";
const POLL_INTERVAL_FALSE_STATE = 50;

async function testGeoSharingIconVisible(state = true) {
  let sharingIcon = document.getElementById("geo-sharing-icon");
  ok(sharingIcon, "Geo sharing icon exists");

  try {
    await BrowserTestUtils.waitForCondition(
      () => sharingIcon.hasAttribute("sharing") === true,
      "Waiting for geo sharing icon visibility state",
      // If we wait for sharing icon to *not* show, waitForCondition will always timeout on correct state.
      // In these cases we want to reduce the wait time from 5 seconds to 2.5 seconds to prevent test duration timeouts
      !state ? POLL_INTERVAL_FALSE_STATE : undefined
    );
  } catch (e) {
    ok(!state, "Geo sharing icon not showing");
    return;
  }
  ok(state, "Geo sharing icon showing");
}

async function checkForDOMElement(state, id) {
  info(`Testing state ${state} of element  ${id}`);
  try {
    await BrowserTestUtils.waitForCondition(
      () => {
        let el = document.getElementById(id);
        return el != null;
      },
      `Waiting for ${id}`,
      !state ? POLL_INTERVAL_FALSE_STATE : undefined
    );
  } catch (e) {
    ok(!state, `${id} has correct state`);
    return;
  }
  ok(state, `${id} has correct state`);
}

async function testIdentityPopupGeoContainer(
  containerVisible,
  timestampVisible
) {
  // Only call openIdentityPopup if popup is closed, otherwise it does not resolve
  if (!gIdentityHandler._identityBox.hasAttribute("open")) {
    await openIdentityPopup();
  }

  let checkContainer = checkForDOMElement(
    containerVisible,
    "identity-popup-geo-container"
  );
  let checkAccessIndicator = checkForDOMElement(
    timestampVisible,
    "geo-access-indicator-item"
  );

  return Promise.all([checkContainer, checkAccessIndicator]);
}

function openExamplePage(tabbrowser = gBrowser) {
  return BrowserTestUtils.openNewForegroundTab(tabbrowser, EXAMPLE_PAGE_URL);
}

function requestGeoLocation(browser) {
  return ContentTask.spawn(browser, null, () => {
    return new Promise(resolve => {
      content.navigator.geolocation.getCurrentPosition(
        () => resolve(true),
        error => resolve(error.code !== 1) // PERMISSION_DENIED = 1
      );
    });
  });
}

function answerGeoLocationPopup(allow, remember = false) {
  let notification = PopupNotifications.getNotification("geolocation");
  ok(
    PopupNotifications.isPanelOpen && notification,
    "Geolocation notification is open"
  );

  let rememberCheck = PopupNotifications.panel.querySelector(
    ".popup-notification-checkbox"
  );
  rememberCheck.checked = remember;

  let popupHidden = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphidden"
  );
  if (allow) {
    let allowBtn = PopupNotifications.panel.querySelector(
      ".popup-notification-primary-button"
    );
    allowBtn.click();
  } else {
    let denyBtn = PopupNotifications.panel.querySelector(
      ".popup-notification-secondary-button"
    );
    denyBtn.click();
  }
  return popupHidden;
}

function setGeoLastAccess(browser, state) {
  return new Promise(resolve => {
    let host = browser.currentURI.host;
    let handler = {
      handleCompletion: () => resolve(),
    };

    if (!state) {
      CP.removeByDomainAndName(
        host,
        GEO_CONTENT_PREF_KEY,
        browser.loadContext,
        handler
      );
      return;
    }
    CP.set(
      host,
      GEO_CONTENT_PREF_KEY,
      new Date().toString(),
      browser.loadContext,
      handler
    );
  });
}

async function testGeoLocationLastAccessSet(browser) {
  let timestamp = await new Promise(resolve => {
    let lastAccess = null;
    CP.getByDomainAndName(
      gBrowser.currentURI.spec,
      GEO_CONTENT_PREF_KEY,
      browser.loadContext,
      {
        handleResult(pref) {
          lastAccess = pref.value;
        },
        handleCompletion() {
          resolve(lastAccess);
        },
      }
    );
  });

  ok(timestamp != null, "Geo last access timestamp set");

  let parseSuccess = true;
  try {
    timestamp = new Date(timestamp);
  } catch (e) {
    parseSuccess = false;
  }
  ok(
    parseSuccess && !isNaN(timestamp),
    "Geo last access timestamp is valid Date"
  );
}

async function cleanup(tab) {
  await setGeoLastAccess(tab.linkedBrowser, false);
  SitePermissions.removeFromPrincipal(
    tab.linkedBrowser.contentPrincipal,
    "geo",
    tab.linkedBrowser
  );
  gBrowser.resetBrowserSharing(tab.linkedBrowser);
  BrowserTestUtils.removeTab(tab);
}

async function testIndicatorGeoSharingState(active) {
  let tab = await openExamplePage();
  gBrowser.updateBrowserSharing(tab.linkedBrowser, { geo: active });
  await testGeoSharingIconVisible(active);

  await cleanup(tab);
}

async function testIndicatorExplicitAllow(persistent) {
  let tab = await openExamplePage();

  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  info("Requesting geolocation");
  let request = requestGeoLocation(tab.linkedBrowser);
  await popupShown;
  info("Allowing geolocation via popup");
  answerGeoLocationPopup(true, persistent);
  await request;

  await Promise.all([
    testGeoSharingIconVisible(true),
    testIdentityPopupGeoContainer(true, true),
    testGeoLocationLastAccessSet(tab.linkedBrowser),
  ]);

  await cleanup(tab);
}

// Indicator and identity popup entry shown after explicit PermissionUI geolocation allow
add_task(function test_indicator_and_timestamp_after_explicit_allow() {
  return testIndicatorExplicitAllow(false);
});
add_task(function test_indicator_and_timestamp_after_explicit_allow_remember() {
  return testIndicatorExplicitAllow(true);
});

// Indicator and identity popup entry shown after auto PermissionUI geolocation allow
add_task(async function test_indicator_and_timestamp_after_implicit_allow() {
  SitePermissions.set(
    EXAMPLE_PAGE_URI,
    "geo",
    SitePermissions.ALLOW,
    SitePermissions.SCOPE_PERSISTENT
  );
  let tab = await openExamplePage();
  let result = await requestGeoLocation(tab.linkedBrowser);
  ok(result, "Request should be allowed");

  await Promise.all([
    testGeoSharingIconVisible(true),
    testIdentityPopupGeoContainer(true, true),
    testGeoLocationLastAccessSet(tab.linkedBrowser),
  ]);

  await cleanup(tab);
});

// Indicator shown when manually setting sharing state to true
add_task(function test_indicator_sharing_state_active() {
  return testIndicatorGeoSharingState(true);
});

// Indicator not shown when manually setting sharing state to false
add_task(function test_indicator_sharing_state_inactive() {
  return testIndicatorGeoSharingState(false);
});

// Identity popup shows permission if geo permission is set to persistent allow
add_task(async function test_identity_popup_permission_scope_permanent() {
  SitePermissions.set(
    EXAMPLE_PAGE_URI,
    "geo",
    SitePermissions.ALLOW,
    SitePermissions.SCOPE_PERSISTENT
  );
  let tab = await openExamplePage();

  await testIdentityPopupGeoContainer(true, false); // Expect permission to be visible, but not lastAccess indicator

  await cleanup(tab);
});

// Sharing state set, but no permission
add_task(async function test_identity_popup_permission_sharing_state() {
  let tab = await openExamplePage();
  gBrowser.updateBrowserSharing(tab.linkedBrowser, { geo: true });
  await testIdentityPopupGeoContainer(true, false);

  await cleanup(tab);
});

// Identity popup has correct state if sharing state and last geo access timestamp are set
add_task(
  async function test_identity_popup_permission_sharing_state_timestamp() {
    let tab = await openExamplePage();
    gBrowser.updateBrowserSharing(tab.linkedBrowser, { geo: true });
    await setGeoLastAccess(tab.linkedBrowser, true);

    await testIdentityPopupGeoContainer(true, true);

    await cleanup(tab);
  }
);

// Clicking permission clear button clears permission and resets geo sharing state
add_task(async function test_identity_popup_permission_clear() {
  SitePermissions.set(
    EXAMPLE_PAGE_URI,
    "geo",
    SitePermissions.ALLOW,
    SitePermissions.SCOPE_PERSISTENT
  );
  let tab = await openExamplePage();
  gBrowser.updateBrowserSharing(tab.linkedBrowser, { geo: true });

  await openIdentityPopup();

  let clearButton = document.querySelector(
    "#identity-popup-geo-container button"
  );
  ok(clearButton, "Clear button is visible");
  clearButton.click();

  await Promise.all([
    testGeoSharingIconVisible(false),
    testIdentityPopupGeoContainer(false, false),
    BrowserTestUtils.waitForCondition(() => {
      let sharingState = tab._sharingState;
      return (
        sharingState == null ||
        sharingState.geo == null ||
        sharingState.geo === false
      );
    }, "Waiting for geo sharing state to reset"),
  ]);
  await cleanup(tab);
});
