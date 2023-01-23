/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

const { PermissionUI } = ChromeUtils.import(
  "resource:///modules/PermissionUI.jsm"
);
const { SitePermissions } = ChromeUtils.import(
  "resource:///modules/SitePermissions.jsm"
);
const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

const CP = Cc["@mozilla.org/content-pref/service;1"].getService(
  Ci.nsIContentPrefService2
);

const EXAMPLE_PAGE_URL = "https://example.com";
const EXAMPLE_PAGE_URI = Services.io.newURI(EXAMPLE_PAGE_URL);
const EXAMPLE_PAGE_PRINCIPAL = Services.scriptSecurityManager.createContentPrincipal(
  EXAMPLE_PAGE_URI,
  {}
);
const GEO_CONTENT_PREF_KEY = "permissions.geoLocation.lastAccess";
const POLL_INTERVAL_FALSE_STATE = 50;

async function testGeoSharingIconVisible(state = true) {
  let sharingIcon = document.getElementById("geo-sharing-icon");
  ok(sharingIcon, "Geo sharing icon exists");

  try {
    await TestUtils.waitForCondition(
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
  let el;
  try {
    await TestUtils.waitForCondition(
      () => {
        el = document.getElementById(id);
        return el != null;
      },
      `Waiting for ${id}`,
      !state ? POLL_INTERVAL_FALSE_STATE : undefined
    );
  } catch (e) {
    ok(!state, `${id} has correct state`);
    return el;
  }
  ok(state, `${id} has correct state`);

  return el;
}

async function testPermissionPopupGeoContainer(
  containerVisible,
  timestampVisible
) {
  // The container holds the timestamp element, therefore we can't have a
  // visible timestamp without the container.
  if (timestampVisible && !containerVisible) {
    ok(false, "Can't have timestamp without container");
  }

  // Only call openPermissionPopup if popup is closed, otherwise it does not resolve
  if (!gPermissionPanel._identityPermissionBox.hasAttribute("open")) {
    await openPermissionPopup();
  }

  let checkContainer = checkForDOMElement(
    containerVisible,
    "permission-popup-geo-container"
  );

  if (containerVisible && timestampVisible) {
    // Wait for the geo container to be fully populated.
    // The time label is computed async.
    let container = await checkContainer;
    await TestUtils.waitForCondition(
      () => container.childElementCount == 2,
      "permission-popup-geo-container should have two elements."
    );
    is(
      container.childNodes[0].classList[0],
      "permission-popup-permission-item",
      "Geo container should have permission item."
    );
    is(
      container.childNodes[1].id,
      "geo-access-indicator-item",
      "Geo container should have indicator item."
    );
  }
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
  return SpecialPowers.spawn(browser, [], () => {
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
    testPermissionPopupGeoContainer(true, true),
    testGeoLocationLastAccessSet(tab.linkedBrowser),
  ]);

  await cleanup(tab);
}

// Indicator and permission popup entry shown after explicit PermissionUI geolocation allow
add_task(function test_indicator_and_timestamp_after_explicit_allow() {
  return testIndicatorExplicitAllow(false);
});
add_task(function test_indicator_and_timestamp_after_explicit_allow_remember() {
  return testIndicatorExplicitAllow(true);
});

// Indicator and permission popup entry shown after auto PermissionUI geolocation allow
add_task(async function test_indicator_and_timestamp_after_implicit_allow() {
  PermissionTestUtils.add(
    EXAMPLE_PAGE_URI,
    "geo",
    Services.perms.ALLOW_ACTION,
    Services.perms.EXPIRE_NEVER
  );
  let tab = await openExamplePage();
  let result = await requestGeoLocation(tab.linkedBrowser);
  ok(result, "Request should be allowed");

  await Promise.all([
    testGeoSharingIconVisible(true),
    testPermissionPopupGeoContainer(true, true),
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

// Permission popup shows permission if geo permission is set to persistent allow
add_task(async function test_permission_popup_permission_scope_permanent() {
  PermissionTestUtils.add(
    EXAMPLE_PAGE_URI,
    "geo",
    Services.perms.ALLOW_ACTION,
    Services.perms.EXPIRE_NEVER
  );
  let tab = await openExamplePage();

  await testPermissionPopupGeoContainer(true, false); // Expect permission to be visible, but not lastAccess indicator

  await cleanup(tab);
});

// Sharing state set, but no permission
add_task(async function test_permission_popup_permission_sharing_state() {
  let tab = await openExamplePage();
  gBrowser.updateBrowserSharing(tab.linkedBrowser, { geo: true });
  await testPermissionPopupGeoContainer(true, false);

  await cleanup(tab);
});

// Permission popup has correct state if sharing state and last geo access timestamp are set
add_task(
  async function test_permission_popup_permission_sharing_state_timestamp() {
    let tab = await openExamplePage();
    gBrowser.updateBrowserSharing(tab.linkedBrowser, { geo: true });
    await setGeoLastAccess(tab.linkedBrowser, true);

    await testPermissionPopupGeoContainer(true, true);

    await cleanup(tab);
  }
);

// Clicking permission clear button clears permission and resets geo sharing state
add_task(async function test_permission_popup_permission_clear() {
  PermissionTestUtils.add(
    EXAMPLE_PAGE_URI,
    "geo",
    Services.perms.ALLOW_ACTION,
    Services.perms.EXPIRE_NEVER
  );
  let tab = await openExamplePage();
  gBrowser.updateBrowserSharing(tab.linkedBrowser, { geo: true });

  await openPermissionPopup();

  let clearButton = document.querySelector(
    "#permission-popup-geo-container button"
  );
  ok(clearButton, "Clear button is visible");
  clearButton.click();

  await Promise.all([
    testGeoSharingIconVisible(false),
    testPermissionPopupGeoContainer(false, false),
    TestUtils.waitForCondition(() => {
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

/**
 * Tests that we only show the last access label once when the sharing
 * state is updated multiple times while the popup is open.
 */
add_task(async function test_permission_no_duplicate_last_access_label() {
  let tab = await openExamplePage();
  await setGeoLastAccess(tab.linkedBrowser, true);
  await openPermissionPopup();
  gBrowser.updateBrowserSharing(tab.linkedBrowser, { geo: true });
  gBrowser.updateBrowserSharing(tab.linkedBrowser, { geo: true });
  await testPermissionPopupGeoContainer(true, true);
  await cleanup(tab);
});
