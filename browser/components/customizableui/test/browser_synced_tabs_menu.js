/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

requestLongerTimeout(2);

let {SyncedTabs} = Cu.import("resource://services-sync/SyncedTabs.jsm", {});
let {UIState} = Cu.import("resource://services-sync/UIState.jsm", {});

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
  let oldInternal = SyncedTabs._internal;
  SyncedTabs._internal = mockedInternal;

  let origNotifyStateUpdated = UIState._internal.notifyStateUpdated;
  // Sync start-up will interfere with our tests, don't let UIState send UI updates.
  UIState._internal.notifyStateUpdated = () => {};

  // Force gSync initialization
  gSync.init();

  registerCleanupFunction(() => {
    UIState._internal.notifyStateUpdated = origNotifyStateUpdated;
    SyncedTabs._internal = oldInternal;
  });
});

// The test expects the about:preferences#sync page to open in the current tab
async function openPrefsFromMenuPanel(expectedPanelId, entryPoint) {
  info("Check Sync button functionality");
  Services.prefs.setCharPref("identity.fxaccounts.remote.signup.uri", "https://example.com/");
  CustomizableUI.addWidgetToArea("sync-button", CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);

  await waitForOverflowButtonShown();

  // check the button's functionality
  await document.getElementById("nav-bar").overflowable.show();

  if (entryPoint == "uitour") {
    UITour.tourBrowsersByWindow.set(window, new Set());
    UITour.tourBrowsersByWindow.get(window).add(gBrowser.selectedBrowser);
  }

  let syncButton = document.getElementById("sync-button");
  ok(syncButton, "The Sync button was added to the Panel Menu");

  let tabsUpdatedPromise = promiseObserverNotified("synced-tabs-menu:test:tabs-updated");
  let syncPanel = document.getElementById("PanelUI-remotetabs");
  let viewShownPromise = BrowserTestUtils.waitForEvent(syncPanel, "ViewShown");
  syncButton.click();
  await Promise.all([tabsUpdatedPromise, viewShownPromise]);
  ok(syncPanel.getAttribute("current"), "Sync Panel is in view");

  // Sync is not configured - verify that state is reflected.
  let subpanel = document.getElementById(expectedPanelId);
  ok(!subpanel.hidden, "sync setup element is visible");

  // Find and click the "setup" button.
  let setupButton = subpanel.querySelector(".PanelUI-remotetabs-button");
  setupButton.click();

  await new Promise(resolve => {
    let handler = async (e) => {
      if (e.originalTarget != gBrowser.selectedBrowser.contentDocument ||
          e.target.location.href == "about:blank") {
        info("Skipping spurious 'load' event for " + e.target.location.href);
        return;
      }
      gBrowser.selectedBrowser.removeEventListener("load", handler, true);
      resolve();
    };
    gBrowser.selectedBrowser.addEventListener("load", handler, true);

  });
  newTab = gBrowser.selectedTab;

  is(gBrowser.currentURI.spec, "about:preferences?entrypoint=" + entryPoint + "#sync",
    "Firefox Sync preference page opened with `menupanel` entrypoint");
  ok(!isOverflowOpen(), "The panel closed");

  if (isOverflowOpen()) {
    await hideOverflow();
  }
}

function hideOverflow() {
  let panelHidePromise = promiseOverflowHidden(window);
  PanelUI.overflowPanel.hidePopup();
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
  gSync.updateAllUI({ status: UIState.STATUS_NOT_CONFIGURED });
  await openPrefsFromMenuPanel("PanelUI-remotetabs-setupsync", "synced-tabs");
});
add_task(asyncCleanup);

// When Sync is configured in an unverified state.
add_task(async function() {
  gSync.updateAllUI({ status: UIState.STATUS_NOT_VERIFIED, email: "foo@bar.com" });
  await openPrefsFromMenuPanel("PanelUI-remotetabs-unverified", "synced-tabs");
});
add_task(asyncCleanup);

// When Sync is configured in a "needs reauthentication" state.
add_task(async function() {
  gSync.updateAllUI({ status: UIState.STATUS_LOGIN_FAILED, email: "foo@bar.com" });
  await openPrefsFromMenuPanel("PanelUI-remotetabs-reauthsync", "synced-tabs");
});

// Test the Connect Another Device button
add_task(async function() {
  Services.prefs.setCharPref("identity.fxaccounts.remote.connectdevice.uri", "http://example.com/connectdevice");

  gSync.updateAllUI({ status: UIState.STATUS_SIGNED_IN, email: "foo@bar.com" });

  let button = document.getElementById("PanelUI-remotetabs-connect-device-button");
  ok(button, "found the button");

  await document.getElementById("nav-bar").overflowable.show();
  button.click();
  // the panel should have been closed.
  ok(!isOverflowOpen(), "click closed the panel");
  // should be a new tab - wait for the load.
  is(gBrowser.tabs.length, 2, "there's a new tab");
  await new Promise(resolve => {
    if (gBrowser.selectedBrowser.currentURI.spec == "about:blank") {
      BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(resolve);
      return;
    }
    // the new tab has already transitioned away from about:blank so we
    // are good to go.
    resolve();
  });

  let expectedUrl = `http://example.com/connectdevice?entrypoint=synced-tabs`;
  is(gBrowser.selectedBrowser.currentURI.spec, expectedUrl, "correct URL");
  gBrowser.removeTab(gBrowser.selectedTab);

  Services.prefs.clearUserPref("identity.fxaccounts.remote.connectdevice.uri");
});

// Test the "Sync Now" button
add_task(async function() {
  gSync.updateAllUI({ status: UIState.STATUS_SIGNED_IN, email: "foo@bar.com" });

  await document.getElementById("nav-bar").overflowable.show();
  let tabsUpdatedPromise = promiseObserverNotified("synced-tabs-menu:test:tabs-updated");
  let syncPanel = document.getElementById("PanelUI-remotetabs");
  let viewShownPromise = BrowserTestUtils.waitForEvent(syncPanel, "ViewShown");
  let syncButton = document.getElementById("sync-button");
  syncButton.click();
  await Promise.all([tabsUpdatedPromise, viewShownPromise]);
  ok(syncPanel.getAttribute("current"), "Sync Panel is in view");

  let subpanel = document.getElementById("PanelUI-remotetabs-main");
  ok(!subpanel.hidden, "main pane is visible");
  let deck = document.getElementById("PanelUI-remotetabs-deck");

  // The widget is still fetching tabs, as we've neutered everything that
  // provides them
  is(deck.selectedIndex, DECKINDEX_FETCHING, "first deck entry is visible");

  // Tell the widget there are tabs available, but with zero clients.
  mockedInternal.getTabClients = () => {
    return Promise.resolve([]);
  };
  mockedInternal.hasSyncedThisSession = true;
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

  let didSync = false;
  let oldDoSync = gSync.doSync;
  gSync.doSync = function() {
    didSync = true;
    gSync.doSync = oldDoSync;
  };

  let syncNowButton = document.getElementById("PanelUI-remotetabs-syncnow");
  is(syncNowButton.disabled, false);
  syncNowButton.click();
  ok(didSync, "clicking the button called the correct function");

  await hideOverflow();
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

  gSync.updateAllUI({ status: UIState.STATUS_SIGNED_IN, email: "foo@bar.com" });

  await document.getElementById("nav-bar").overflowable.show();
  let tabsUpdatedPromise = promiseObserverNotified("synced-tabs-menu:test:tabs-updated");
  let syncPanel = document.getElementById("PanelUI-remotetabs");
  let viewShownPromise = BrowserTestUtils.waitForEvent(syncPanel, "ViewShown");
  let syncButton = document.getElementById("sync-button");
  syncButton.click();
  await Promise.all([tabsUpdatedPromise, viewShownPromise]);

  // Check pre-conditions
  ok(syncPanel.getAttribute("current"), "Sync Panel is in view");
  let subpanel = document.getElementById("PanelUI-remotetabs-main");
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

  await hideOverflow();
});
