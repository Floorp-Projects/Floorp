// Test the SyncedTabs counters in BrowserUITelemetry.
"use strict";

const { BrowserUITelemetry: BUIT } = Cu.import("resource:///modules/BrowserUITelemetry.jsm", {});
const {SyncedTabs} = Cu.import("resource://services-sync/SyncedTabs.jsm", {});

function mockSyncedTabs() {
  // Mock SyncedTabs.jsm
  let mockedInternal = {
    get isConfiguredToSyncTabs() { return true; },
    getTabClients() {
      return Promise.resolve([
        {
          id: "guid_desktop",
          type: "client",
          name: "My Desktop",
          tabs: [
            {
              title: "http://example.com/10",
              lastUsed: 10, // the most recent
            },
          ],
        }
      ]);
    },
    syncTabs() {
      return Promise.resolve();
    },
    hasSyncedThisSession: true,
  };

  let oldInternal = SyncedTabs._internal;
  SyncedTabs._internal = mockedInternal;

  // configure our broadcasters so we are in the right state.
  document.getElementById("sync-reauth-state").hidden = true;
  document.getElementById("sync-setup-state").hidden = true;
  document.getElementById("sync-syncnow-state").hidden = false;

  registerCleanupFunction(() => {
    SyncedTabs._internal = oldInternal;

    document.getElementById("sync-reauth-state").hidden = true;
    document.getElementById("sync-setup-state").hidden = false;
    document.getElementById("sync-syncnow-state").hidden = true;
  });
}

mockSyncedTabs();

function promiseTabsUpdated() {
  return new Promise(resolve => {
    Services.obs.addObserver(function onNotification(aSubject, aTopic, aData) {
      Services.obs.removeObserver(onNotification, aTopic);
      resolve();
    }, "synced-tabs-menu:test:tabs-updated");
  });
}

add_task(function* test_menu() {
  // Reset BrowserUITelemetry's world.
  BUIT._countableEvents = {};

  let tabsUpdated = promiseTabsUpdated();

  // check the button's functionality
  yield PanelUI.show();

  let syncButton = document.getElementById("sync-button");
  syncButton.click();

  yield tabsUpdated;
  // Get our 1 tab and click on it.
  let tabList = document.getElementById("PanelUI-remotetabs-tabslist");
  let tabEntry = tabList.firstChild.nextSibling;
  tabEntry.click();

  let counts = BUIT._countableEvents[BUIT.currentBucket];
  Assert.deepEqual(counts, {
    "click-builtin-item": { "sync-button": { left: 1 } },
    "synced-tabs": { open: { "toolbarbutton-subview": 1 } },
  });
});

add_task(function* test_sidebar() {
  // Reset BrowserUITelemetry's world.
  BUIT._countableEvents = {};

  yield SidebarUI.show("viewTabsSidebar");

  let syncedTabsDeckComponent = SidebarUI.browser.contentWindow.syncedTabsDeckComponent;

  syncedTabsDeckComponent._accountStatus = () => Promise.resolve(true);

  // Once the tabs container has been selected (which here means "'selected'
  // added to the class list") we are ready to test.
  let container = SidebarUI.browser.contentDocument.querySelector(".tabs-container");
  let promiseUpdated = BrowserTestUtils.waitForAttribute("class", container);

  yield syncedTabsDeckComponent.updatePanel();
  yield promiseUpdated;

  let selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");
  let tab = selectedPanel.querySelector(".tab");
  tab.click();
  let counts = BUIT._countableEvents[BUIT.currentBucket];
  Assert.deepEqual(counts, {
    sidebar: {
      viewTabsSidebar: { show: 1 },
    },
    "synced-tabs": { open: { sidebar: 1 } }
  });
  yield SidebarUI.hide();
});
