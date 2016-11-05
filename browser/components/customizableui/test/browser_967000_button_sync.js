/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

requestLongerTimeout(2);

let {SyncedTabs} = Cu.import("resource://services-sync/SyncedTabs.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "UITour", "resource:///modules/UITour.jsm");

// These are available on the widget implementation, but it seems impossible
// to grab that impl at runtime.
const DECKINDEX_TABS = 0;
const DECKINDEX_TABSDISABLED = 1;
const DECKINDEX_FETCHING = 2;
const DECKINDEX_NOCLIENTS = 3;

var initialLocation = gBrowser.currentURI.spec;
var newTab = null;

// A helper to notify there are new tabs. Returns a promise that is resolved
// once the UI has been updated.
function updateTabsPanel() {
  let promiseTabsUpdated = promiseObserverNotified("synced-tabs-menu:test:tabs-updated");
  Services.obs.notifyObservers(null, SyncedTabs.TOPIC_TABS_CHANGED, null);
  return promiseTabsUpdated;
}

// This is the mock we use for SyncedTabs.jsm - tests may override various
// functions.
let mockedInternal = {
  get isConfiguredToSyncTabs() { return true; },
  getTabClients() { return []; },
  syncTabs() {},
  hasSyncedThisSession: false,
};


add_task(function* setup() {
  let oldInternal = SyncedTabs._internal;
  SyncedTabs._internal = mockedInternal;

  registerCleanupFunction(() => {
    SyncedTabs._internal = oldInternal;
  });
});

// The test expects the about:preferences#sync page to open in the current tab
function* openPrefsFromMenuPanel(expectedPanelId, entryPoint) {
  info("Check Sync button functionality");
  Services.prefs.setCharPref("identity.fxaccounts.remote.signup.uri", "http://example.com/");

  // add the Sync button to the panel
  CustomizableUI.addWidgetToArea("sync-button", CustomizableUI.AREA_PANEL);

  // check the button's functionality
  yield PanelUI.show();

  if (entryPoint == "uitour") {
    UITour.tourBrowsersByWindow.set(window, new Set());
    UITour.tourBrowsersByWindow.get(window).add(gBrowser.selectedBrowser);
  }

  let syncButton = document.getElementById("sync-button");
  ok(syncButton, "The Sync button was added to the Panel Menu");

  syncButton.click();
  let syncPanel = document.getElementById("PanelUI-remotetabs");
  ok(syncPanel.getAttribute("current"), "Sync Panel is in view");

  // Sync is not configured - verify that state is reflected.
  let subpanel = document.getElementById(expectedPanelId)
  ok(!subpanel.hidden, "sync setup element is visible");

  // Find and click the "setup" button.
  let setupButton = subpanel.querySelector(".PanelUI-remotetabs-prefs-button");
  setupButton.click();

  let deferred = Promise.defer();
  let handler = (e) => {
    if (e.originalTarget != gBrowser.selectedBrowser.contentDocument ||
        e.target.location.href == "about:blank") {
      info("Skipping spurious 'load' event for " + e.target.location.href);
      return;
    }
    gBrowser.selectedBrowser.removeEventListener("load", handler, true);
    deferred.resolve();
  }
  gBrowser.selectedBrowser.addEventListener("load", handler, true);

  yield deferred.promise;
  newTab = gBrowser.selectedTab;

  is(gBrowser.currentURI.spec, "about:preferences?entrypoint=" + entryPoint + "#sync",
    "Firefox Sync preference page opened with `menupanel` entrypoint");
  ok(!isPanelUIOpen(), "The panel closed");

  if (isPanelUIOpen()) {
    let panelHidePromise = promisePanelHidden(window);
    PanelUI.hide();
    yield panelHidePromise;
  }
}

function* asyncCleanup() {
  Services.prefs.clearUserPref("identity.fxaccounts.remote.signup.uri");
  // reset the panel UI to the default state
  yield resetCustomization();
  ok(CustomizableUI.inDefaultState, "The panel UI is in default state again.");

  // restore the tabs
  gBrowser.addTab(initialLocation);
  gBrowser.removeTab(newTab);
  UITour.tourBrowsersByWindow.delete(window);
}

// When Sync is not setup.
add_task(() => openPrefsFromMenuPanel("PanelUI-remotetabs-setupsync", "synced-tabs"));
add_task(asyncCleanup);

// When Sync is configured in a "needs reauthentication" state.
add_task(function* () {
  // configure our broadcasters so we are in the right state.
  document.getElementById("sync-reauth-state").hidden = false;
  document.getElementById("sync-setup-state").hidden = true;
  document.getElementById("sync-syncnow-state").hidden = true;
  yield openPrefsFromMenuPanel("PanelUI-remotetabs-reauthsync", "synced-tabs")
});

// Test the mobile promo links
add_task(function* () {
  // change the preferences for the mobile links.
  Services.prefs.setCharPref("identity.mobilepromo.android", "http://example.com/?os=android&tail=");
  Services.prefs.setCharPref("identity.mobilepromo.ios", "http://example.com/?os=ios&tail=");

  mockedInternal.getTabClients = () => [];
  mockedInternal.syncTabs = () => Promise.resolve();

  document.getElementById("sync-reauth-state").hidden = true;
  document.getElementById("sync-setup-state").hidden = true;
  document.getElementById("sync-syncnow-state").hidden = false;

  CustomizableUI.addWidgetToArea("sync-button", CustomizableUI.AREA_PANEL);

  let syncPanel = document.getElementById("PanelUI-remotetabs");
  let links = syncPanel.querySelectorAll(".remotetabs-promo-link");

  is(links.length, 2, "found 2 links as expected");

  // test each link and left and middle mouse buttons
  for (let link of links) {
    for (let button = 0; button < 2; button++) {
      yield PanelUI.show();
      EventUtils.sendMouseEvent({ type: "click", button }, link, window);
      // the panel should have been closed.
      ok(!isPanelUIOpen(), "click closed the panel");
      // should be a new tab - wait for the load.
      is(gBrowser.tabs.length, 2, "there's a new tab");
      yield new Promise(resolve => {
        if (gBrowser.selectedBrowser.currentURI.spec == "about:blank") {
          gBrowser.selectedBrowser.addEventListener("load", function listener(e) {
            gBrowser.selectedBrowser.removeEventListener("load", listener, true);
            resolve();
          }, true);
          return;
        }
        // the new tab has already transitioned away from about:blank so we
        // are good to go.
        resolve();
      });

      let os = link.getAttribute("mobile-promo-os");
      let expectedUrl = `http://example.com/?os=${os}&tail=synced-tabs`;
      is(gBrowser.selectedBrowser.currentURI.spec, expectedUrl, "correct URL");
      gBrowser.removeTab(gBrowser.selectedTab);
    }
  }

  // test each link and right mouse button - should be a noop.
  yield PanelUI.show();
  for (let link of links) {
    EventUtils.sendMouseEvent({ type: "click", button: 2 }, link, window);
    // the panel should still be open
    ok(isPanelUIOpen(), "panel remains open after right-click");
    is(gBrowser.tabs.length, 1, "no new tab was opened");
  }
  PanelUI.hide();

  Services.prefs.clearUserPref("identity.mobilepromo.android");
  Services.prefs.clearUserPref("identity.mobilepromo.ios");
});

// Test the "Sync Now" button
add_task(function* () {
  mockedInternal.getTabClients = () => [];
  mockedInternal.syncTabs = () => {
    return Promise.resolve();
  }

  // configure our broadcasters so we are in the right state.
  document.getElementById("sync-reauth-state").hidden = true;
  document.getElementById("sync-setup-state").hidden = true;
  document.getElementById("sync-syncnow-state").hidden = false;

  // add the Sync button to the panel
  CustomizableUI.addWidgetToArea("sync-button", CustomizableUI.AREA_PANEL);
  yield PanelUI.show();
  document.getElementById("sync-button").click();
  let syncPanel = document.getElementById("PanelUI-remotetabs");
  ok(syncPanel.getAttribute("current"), "Sync Panel is in view");

  let subpanel = document.getElementById("PanelUI-remotetabs-main")
  ok(!subpanel.hidden, "main pane is visible");
  let deck = document.getElementById("PanelUI-remotetabs-deck");

  // The widget is still fetching tabs, as we've neutered everything that
  // provides them
  is(deck.selectedIndex, DECKINDEX_FETCHING, "first deck entry is visible");

  let syncNowButton = document.getElementById("PanelUI-remotetabs-syncnow");

  let didSync = false;
  let oldDoSync = gSyncUI.doSync;
  gSyncUI.doSync = function() {
    didSync = true;
    mockedInternal.hasSyncedThisSession = true;
    gSyncUI.doSync = oldDoSync;
  }
  syncNowButton.click();
  ok(didSync, "clicking the button called the correct function");

  // Tell the widget there are tabs available, but with zero clients.
  mockedInternal.getTabClients = () => {
    return Promise.resolve([]);
  }
  yield updateTabsPanel();
  // The UI should be showing the "no clients" pane.
  is(deck.selectedIndex, DECKINDEX_NOCLIENTS, "no-clients deck entry is visible");

  // Tell the widget there are tabs available - we have 3 clients, one with no
  // tabs.
  mockedInternal.getTabClients = () => {
    return Promise.resolve([
      {
        id: "guid_mobile",
        type: "client",
        name: "My Phone",
        tabs: [],
      },
      {
        id: "guid_desktop",
        type: "client",
        name: "My Desktop",
        tabs: [
          {
            title: "http://example.com/10",
            lastUsed: 10, // the most recent
          },
          {
            title: "http://example.com/1",
            lastUsed: 1, // the least recent.
          },
          {
            title: "http://example.com/5",
            lastUsed: 5,
          },
        ],
      },
      {
        id: "guid_second_desktop",
        name: "My Other Desktop",
        tabs: [
          {
            title: "http://example.com/6",
            lastUsed: 6,
          }
        ],
      },
    ]);
  };
  yield updateTabsPanel();

  // The UI should be showing tabs!
  is(deck.selectedIndex, DECKINDEX_TABS, "no-clients deck entry is visible");
  let tabList = document.getElementById("PanelUI-remotetabs-tabslist");
  let node = tabList.firstChild;
  // First entry should be the client with the most-recent tab.
  is(node.getAttribute("itemtype"), "client", "node is a client entry");
  is(node.textContent, "My Desktop", "correct client");
  // Next entry is the most-recent tab
  node = node.nextSibling;
  is(node.getAttribute("itemtype"), "tab", "node is a tab");
  is(node.getAttribute("label"), "http://example.com/10");

  // Next entry is the next-most-recent tab
  node = node.nextSibling;
  is(node.getAttribute("itemtype"), "tab", "node is a tab");
  is(node.getAttribute("label"), "http://example.com/5");

  // Next entry is the least-recent tab from the first client.
  node = node.nextSibling;
  is(node.getAttribute("itemtype"), "tab", "node is a tab");
  is(node.getAttribute("label"), "http://example.com/1");

  // Next is a menuseparator between the clients.
  node = node.nextSibling;
  is(node.nodeName, "menuseparator");

  // Next is the client with 1 tab.
  node = node.nextSibling;
  is(node.getAttribute("itemtype"), "client", "node is a client entry");
  is(node.textContent, "My Other Desktop", "correct client");
  // Its single tab
  node = node.nextSibling;
  is(node.getAttribute("itemtype"), "tab", "node is a tab");
  is(node.getAttribute("label"), "http://example.com/6");

  // Next is a menuseparator between the clients.
  node = node.nextSibling;
  is(node.nodeName, "menuseparator");

  // Next is the client with no tab.
  node = node.nextSibling;
  is(node.getAttribute("itemtype"), "client", "node is a client entry");
  is(node.textContent, "My Phone", "correct client");
  // There is a single node saying there's no tabs for the client.
  node = node.nextSibling;
  is(node.nodeName, "label", "node is a label");
  is(node.getAttribute("itemtype"), "", "node is neither a tab nor a client");

  node = node.nextSibling;
  is(node, null, "no more entries");
});
