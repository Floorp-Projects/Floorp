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
  Services.obs.notifyObservers(null, SyncedTabs.TOPIC_TABS_CHANGED);
  return promiseTabsUpdated;
}

// This is the mock we use for SyncedTabs.jsm - tests may override various
// functions.
let mockedInternal = {
  get isConfiguredToSyncTabs() { return true; },
  getTabClients() { return Promise.resolve([]); },
  syncTabs() { return Promise.resolve(); },
  hasSyncedThisSession: false,
};


add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({set: [["browser.photon.structure.enabled", false]]});
  let oldInternal = SyncedTabs._internal;
  SyncedTabs._internal = mockedInternal;

  // This test hacks some observer states to simulate a user being signed
  // in to Sync - restore them when the test completes.
  let initialObserverStates = {};
  for (let id of ["sync-reauth-state", "sync-setup-state", "sync-syncnow-state"]) {
    initialObserverStates[id] = document.getElementById(id).hidden;
  }

  registerCleanupFunction(() => {
    SyncedTabs._internal = oldInternal;
    for (let [id, initial] of Object.entries(initialObserverStates)) {
      document.getElementById(id).hidden = initial;
    }
  });
});

// The test expects the about:preferences#sync page to open in the current tab
async function openPrefsFromMenuPanel(expectedPanelId, entryPoint) {
  info("Check Sync button functionality");
  Services.prefs.setCharPref("identity.fxaccounts.remote.signup.uri", "http://example.com/");

  // check the button's functionality
  await PanelUI.show();

  if (entryPoint == "uitour") {
    UITour.tourBrowsersByWindow.set(window, new Set());
    UITour.tourBrowsersByWindow.get(window).add(gBrowser.selectedBrowser);
  }

  let syncButton = document.getElementById("sync-button");
  ok(syncButton, "The Sync button was added to the Panel Menu");

  let tabsUpdatedPromise = promiseObserverNotified("synced-tabs-menu:test:tabs-updated");
  syncButton.click();
  await tabsUpdatedPromise;
  let syncPanel = document.getElementById("PanelUI-remotetabs");
  ok(syncPanel.getAttribute("current"), "Sync Panel is in view");

  // Sync is not configured - verify that state is reflected.
  let subpanel = document.getElementById(expectedPanelId)
  ok(!subpanel.hidden, "sync setup element is visible");

  // Find and click the "setup" button.
  let setupButton = subpanel.querySelector(".PanelUI-remotetabs-prefs-button");
  setupButton.click();

  await new Promise(resolve => {
    let handler = (e) => {
      if (e.originalTarget != gBrowser.selectedBrowser.contentDocument ||
          e.target.location.href == "about:blank") {
        info("Skipping spurious 'load' event for " + e.target.location.href);
        return;
      }
      gBrowser.selectedBrowser.removeEventListener("load", handler, true);
      resolve();
    }
    gBrowser.selectedBrowser.addEventListener("load", handler, true);

  });
  newTab = gBrowser.selectedTab;

  is(gBrowser.currentURI.spec, "about:preferences?entrypoint=" + entryPoint + "#sync",
    "Firefox Sync preference page opened with `menupanel` entrypoint");
  ok(!isPanelUIOpen(), "The panel closed");

  if (isPanelUIOpen()) {
    await panelUIHide();
  }
}

function panelUIHide() {
  let panelHidePromise = promisePanelHidden(window);
  PanelUI.hide();
  return panelHidePromise;
}

async function asyncCleanup() {
  Services.prefs.clearUserPref("identity.fxaccounts.remote.signup.uri");
  // reset the panel UI to the default state
  await resetCustomization();
  ok(CustomizableUI.inDefaultState, "The panel UI is in default state again.");

  // restore the tabs
  BrowserTestUtils.addTab(gBrowser, initialLocation);
  gBrowser.removeTab(newTab);
  UITour.tourBrowsersByWindow.delete(window);
}

// When Sync is not setup.
add_task(async function() {
  document.getElementById("sync-reauth-state").hidden = true;
  document.getElementById("sync-setup-state").hidden = false;
  document.getElementById("sync-syncnow-state").hidden = true;
  await openPrefsFromMenuPanel("PanelUI-remotetabs-setupsync", "synced-tabs")
});
add_task(asyncCleanup);

// When Sync is configured in a "needs reauthentication" state.
add_task(async function() {
  // configure our broadcasters so we are in the right state.
  document.getElementById("sync-reauth-state").hidden = false;
  document.getElementById("sync-setup-state").hidden = true;
  document.getElementById("sync-syncnow-state").hidden = true;
  await openPrefsFromMenuPanel("PanelUI-remotetabs-reauthsync", "synced-tabs")
});

// Test the mobile promo links
add_task(async function() {
  // change the preferences for the mobile links.
  Services.prefs.setCharPref("identity.mobilepromo.android", "http://example.com/?os=android&tail=");
  Services.prefs.setCharPref("identity.mobilepromo.ios", "http://example.com/?os=ios&tail=");

  document.getElementById("sync-reauth-state").hidden = true;
  document.getElementById("sync-setup-state").hidden = true;
  document.getElementById("sync-syncnow-state").hidden = false;

  let syncPanel = document.getElementById("PanelUI-remotetabs");
  let links = syncPanel.querySelectorAll(".remotetabs-promo-link");

  is(links.length, 2, "found 2 links as expected");

  // test each link and left and middle mouse buttons
  for (let link of links) {
    for (let button = 0; button < 2; button++) {
      await PanelUI.show();
      EventUtils.sendMouseEvent({ type: "click", button }, link, window);
      // the panel should have been closed.
      ok(!isPanelUIOpen(), "click closed the panel");
      // should be a new tab - wait for the load.
      is(gBrowser.tabs.length, 2, "there's a new tab");
      await new Promise(resolve => {
        if (gBrowser.selectedBrowser.currentURI.spec == "about:blank") {
          gBrowser.selectedBrowser.addEventListener("load", function(e) {
            resolve();
          }, {capture: true, once: true});
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
  await PanelUI.show();
  for (let link of links) {
    EventUtils.sendMouseEvent({ type: "click", button: 2 }, link, window);
    // the panel should still be open
    ok(isPanelUIOpen(), "panel remains open after right-click");
    is(gBrowser.tabs.length, 1, "no new tab was opened");
  }
  await panelUIHide();

  Services.prefs.clearUserPref("identity.mobilepromo.android");
  Services.prefs.clearUserPref("identity.mobilepromo.ios");
});

// Test the "Sync Now" button
add_task(async function() {
  // configure our broadcasters so we are in the right state.
  document.getElementById("sync-reauth-state").hidden = true;
  document.getElementById("sync-setup-state").hidden = true;
  document.getElementById("sync-syncnow-state").hidden = false;

  await PanelUI.show();
  let tabsUpdatedPromise = promiseObserverNotified("synced-tabs-menu:test:tabs-updated");
  document.getElementById("sync-button").click();
  await tabsUpdatedPromise;
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
  let oldDoSync = gSync.doSync;
  gSync.doSync = function() {
    didSync = true;
    mockedInternal.hasSyncedThisSession = true;
    gSync.doSync = oldDoSync;
  }
  syncNowButton.click();
  ok(didSync, "clicking the button called the correct function");

  // Tell the widget there are tabs available, but with zero clients.
  mockedInternal.getTabClients = () => {
    return Promise.resolve([]);
  }
  await updateTabsPanel();
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
        lastModified: 1492201200,
        tabs: [],
      },
      {
        id: "guid_desktop",
        type: "client",
        name: "My Desktop",
        lastModified: 1492201200,
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
        lastModified: 1492201200,
        tabs: [
          {
            title: "http://example.com/6",
            lastUsed: 6,
          }
        ],
      },
    ]);
  };
  await updateTabsPanel();

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

  await panelUIHide();
});

// Test the pagination capabilities (Show More/All tabs)
add_task(async function() {
  mockedInternal.getTabClients = () => {
    return Promise.resolve([
      {
        id: "guid_desktop",
        type: "client",
        name: "My Desktop",
        lastModified: 1492201200,
        tabs: function() {
          let allTabsDesktop = [];
          // We choose 77 tabs, because TABS_PER_PAGE is 25, which means
          // on the second to last page we should have 22 items shown
          // (because we have to show at least NEXT_PAGE_MIN_TABS=5 tabs on the last page)
          for (let i = 1; i <= 77; i++) {
            allTabsDesktop.push({ title: "Tab #" + i });
          }
          return allTabsDesktop;
        }(),
      }
    ]);
  };

  // configure our broadcasters so we are in the right state.
  document.getElementById("sync-reauth-state").hidden = true;
  document.getElementById("sync-setup-state").hidden = true;
  document.getElementById("sync-syncnow-state").hidden = false;

  await PanelUI.show();
  let tabsUpdatedPromise = promiseObserverNotified("synced-tabs-menu:test:tabs-updated");
  document.getElementById("sync-button").click();
  await tabsUpdatedPromise;

  // Check pre-conditions
  let syncPanel = document.getElementById("PanelUI-remotetabs");
  ok(syncPanel.getAttribute("current"), "Sync Panel is in view");
  let subpanel = document.getElementById("PanelUI-remotetabs-main")
  ok(!subpanel.hidden, "main pane is visible");
  let deck = document.getElementById("PanelUI-remotetabs-deck");
  is(deck.selectedIndex, DECKINDEX_TABS, "we should be showing tabs");

  function checkTabsPage(tabsShownCount, showMoreLabel) {
    let tabList = document.getElementById("PanelUI-remotetabs-tabslist");
    let node = tabList.firstChild;
    is(node.getAttribute("itemtype"), "client", "node is a client entry");
    is(node.textContent, "My Desktop", "correct client");
    for (let i = 0; i < tabsShownCount; i++) {
      node = node.nextSibling;
      is(node.getAttribute("itemtype"), "tab", "node is a tab");
      is(node.getAttribute("label"), "Tab #" + (i + 1), "the tab is the correct one");
    }
    let showMoreButton;
    if (showMoreLabel) {
      node = showMoreButton = node.nextSibling;
      is(node.getAttribute("itemtype"), "showmorebutton", "node is a show more button");
      is(node.getAttribute("label"), showMoreLabel);
    }
    node = node.nextSibling;
    is(node, null, "no more entries");

    return showMoreButton;
  }

  let showMoreButton;
  function clickShowMoreButton() {
    let promise = promiseObserverNotified("synced-tabs-menu:test:tabs-updated");
    showMoreButton.click();
    return promise;
  }

  showMoreButton = checkTabsPage(25, "Show More");
  await clickShowMoreButton();

  showMoreButton = checkTabsPage(50, "Show More");
  await clickShowMoreButton();

  showMoreButton = checkTabsPage(72, "Show All");
  await clickShowMoreButton();

  checkTabsPage(77, null);

  await panelUIHide();
});
