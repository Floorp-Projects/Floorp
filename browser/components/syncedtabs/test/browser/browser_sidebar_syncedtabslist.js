"use strict";

const FIXTURE = [
  {
    "id": "7cqCr77ptzX3",
    "type": "client",
    "name": "zcarter's Nightly on MacBook-Pro-25",
    "icon": "chrome://browser/skin/sync-desktopIcon.png",
    "tabs": [
      {
        "type": "tab",
        "title": "Firefox for Android — Mobile Web browser — More ways to customize and protect your privacy — Mozilla",
        "url": "https://www.mozilla.org/en-US/firefox/android/?utm_source=firefox-browser&utm_medium=firefox-browser&utm_campaign=synced-tabs-sidebar",
        "icon": "chrome://mozapps/skin/places/defaultFavicon.png",
        "client": "7cqCr77ptzX3",
        "lastUsed": 1452124677
      }
    ]
  },
  {
    "id": "2xU5h-4bkWqA",
    "type": "client",
    "name": "laptop",
    "icon": "chrome://browser/skin/sync-desktopIcon.png",
    "tabs": [
      {
        "type": "tab",
        "title": "Firefox for iOS — Mobile Web browser for your iPhone, iPad and iPod touch — Mozilla",
        "url": "https://www.mozilla.org/en-US/firefox/ios/?utm_source=firefox-browser&utm_medium=firefox-browser&utm_campaign=synced-tabs-sidebar",
        "icon": "moz-anno:favicon:https://www.mozilla.org/media/img/firefox/favicon.dc6635050bf5.ico",
        "client": "2xU5h-4bkWqA",
        "lastUsed": 1451519425
      },
      {
        "type": "tab",
        "title": "Firefox Nightly First Run Page",
        "url": "https://www.mozilla.org/en-US/firefox/nightly/firstrun/?oldversion=45.0a1",
        "icon": "moz-anno:favicon:https://www.mozilla.org/media/img/firefox/favicon-nightly.560395bbb2e1.png",
        "client": "2xU5h-4bkWqA",
        "lastUsed": 1451519420
      }
    ]
  },
  {
    "id": "OL3EJCsdb2JD",
    "type": "client",
    "name": "desktop",
    "icon": "chrome://browser/skin/sync-desktopIcon.png",
    "tabs": []
  }
];

let originalSyncedTabsInternal = null;

function* testClean() {
  let syncedTabsDeckComponent = window.SidebarUI.browser.contentWindow.syncedTabsDeckComponent;
  let SyncedTabs = window.SidebarUI.browser.contentWindow.SyncedTabs;
  syncedTabsDeckComponent._accountStatus.restore();
  SyncedTabs._internal.getTabClients.restore();
  SyncedTabs._internal = originalSyncedTabsInternal;

  yield new Promise(resolve => {
    window.SidebarUI.browser.contentWindow.addEventListener("unload", function listener() {
      window.SidebarUI.browser.contentWindow.removeEventListener("unload", listener);
      resolve();
    });
    SidebarUI.hide();
  });
}

add_task(function* testSyncedTabsSidebarList() {
  yield SidebarUI.show('viewTabsSidebar');

  Assert.equal(SidebarUI.currentID, "viewTabsSidebar", "Sidebar should have SyncedTabs loaded");

  let syncedTabsDeckComponent = SidebarUI.browser.contentWindow.syncedTabsDeckComponent;
  let SyncedTabs = SidebarUI.browser.contentWindow.SyncedTabs;

  Assert.ok(syncedTabsDeckComponent, "component exists");

  originalSyncedTabsInternal = SyncedTabs._internal;
  SyncedTabs._internal = {
    isConfiguredToSyncTabs: true,
    hasSyncedThisSession: true,
    getTabClients() { return Promise.resolve([])},
    syncTabs() {return Promise.resolve();},
  };

  sinon.stub(syncedTabsDeckComponent, "_accountStatus", ()=> Promise.resolve(true));
  sinon.stub(SyncedTabs._internal, "getTabClients", ()=> Promise.resolve(FIXTURE));

  yield syncedTabsDeckComponent.updatePanel();
  // This is a hacky way of waiting for the view to render. The view renders
  // after the following promise (a different instance of which is triggered
  // in updatePanel) resolves, so we wait for it here as well
  yield syncedTabsDeckComponent.tabListComponent._store.getData();

  Assert.ok(SyncedTabs._internal.getTabClients.called, "get clients called");

  let selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");


  Assert.ok(selectedPanel.classList.contains("tabs-container"),
    "tabs panel is selected");

  Assert.equal(selectedPanel.querySelectorAll(".tab").length, 3,
    "three tabs listed");
  Assert.equal(selectedPanel.querySelectorAll(".client").length, 3,
    "three clients listed");
  Assert.equal(selectedPanel.querySelectorAll(".client")[2].querySelectorAll(".empty").length, 1,
    "third client is empty");

  Array.prototype.forEach.call(selectedPanel.querySelectorAll(".client"), (clientNode, i) => {
    checkItem(clientNode, FIXTURE[i]);
    Array.prototype.forEach.call(clientNode.querySelectorAll(".tab"), (tabNode, j) => {
      checkItem(tabNode, FIXTURE[i].tabs[j]);
    });
  });

});

add_task(testClean);

add_task(function* testSyncedTabsSidebarFilteredList() {
  yield SidebarUI.show('viewTabsSidebar');
  let syncedTabsDeckComponent = window.SidebarUI.browser.contentWindow.syncedTabsDeckComponent;
  let SyncedTabs = window.SidebarUI.browser.contentWindow.SyncedTabs;

  Assert.ok(syncedTabsDeckComponent, "component exists");

  originalSyncedTabsInternal = SyncedTabs._internal;
  SyncedTabs._internal = {
    isConfiguredToSyncTabs: true,
    hasSyncedThisSession: true,
    getTabClients() { return Promise.resolve([])},
    syncTabs() {return Promise.resolve();},
  };

  sinon.stub(syncedTabsDeckComponent, "_accountStatus", ()=> Promise.resolve(true));
  sinon.stub(SyncedTabs._internal, "getTabClients", ()=> Promise.resolve(FIXTURE));

  yield syncedTabsDeckComponent.updatePanel();
  // This is a hacky way of waiting for the view to render. The view renders
  // after the following promise (a different instance of which is triggered
  // in updatePanel) resolves, so we wait for it here as well
  yield syncedTabsDeckComponent.tabListComponent._store.getData();

  let filterInput = syncedTabsDeckComponent.container.querySelector(".tabsFilter");
  filterInput.value = "filter text";
  filterInput.blur();

  yield syncedTabsDeckComponent.tabListComponent._store.getData("filter text");

  let selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");
  Assert.ok(selectedPanel.classList.contains("tabs-container"),
    "tabs panel is selected");

  Assert.equal(selectedPanel.querySelectorAll(".tab").length, 3,
    "three tabs listed");
  Assert.equal(selectedPanel.querySelectorAll(".client").length, 0,
    "no clients are listed");

  Assert.equal(filterInput.value, "filter text",
    "filter text box has correct value");

  let FIXTURE_TABS = FIXTURE.reduce((prev, client) => prev.concat(client.tabs), []);

  Array.prototype.forEach.call(selectedPanel.querySelectorAll(".tab"), (tabNode, i) => {
    checkItem(tabNode, FIXTURE_TABS[i]);
  });

});

add_task(testClean);

add_task(function* testSyncedTabsSidebarStatus() {
  let accountExists = false;

  yield SidebarUI.show('viewTabsSidebar');
  let syncedTabsDeckComponent = window.SidebarUI.browser.contentWindow.syncedTabsDeckComponent;
  let SyncedTabs = window.SidebarUI.browser.contentWindow.SyncedTabs;

  originalSyncedTabsInternal = SyncedTabs._internal;
  SyncedTabs._internal = {
    isConfiguredToSyncTabs: false,
    hasSyncedThisSession: false,
    getTabClients() {},
    syncTabs() {return Promise.resolve();},
  };

  Assert.ok(syncedTabsDeckComponent, "component exists");

  sinon.spy(syncedTabsDeckComponent, "updatePanel");
  sinon.spy(syncedTabsDeckComponent, "observe");

  sinon.stub(syncedTabsDeckComponent, "_accountStatus", ()=> Promise.reject("Test error"));
  yield syncedTabsDeckComponent.updatePanel();

  let selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");
  Assert.ok(selectedPanel.classList.contains("notAuthedInfo"),
    "not-authed panel is selected on auth error");

  syncedTabsDeckComponent._accountStatus.restore();
  sinon.stub(syncedTabsDeckComponent, "_accountStatus", ()=> Promise.resolve(accountExists));
  yield syncedTabsDeckComponent.updatePanel();
  selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");
  Assert.ok(selectedPanel.classList.contains("notAuthedInfo"),
    "not-authed panel is selected");

  accountExists = true;
  yield syncedTabsDeckComponent.updatePanel();
  selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");
  Assert.ok(selectedPanel.classList.contains("tabs-disabled"),
    "tabs disabled panel is selected");

  SyncedTabs._internal.isConfiguredToSyncTabs = true;
  yield syncedTabsDeckComponent.updatePanel();
  selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");
  Assert.ok(selectedPanel.classList.contains("tabs-fetching"),
    "tabs fetch panel is selected");

  SyncedTabs._internal.hasSyncedThisSession = true;
  sinon.stub(SyncedTabs._internal, "getTabClients", ()=> Promise.resolve([]));
  yield syncedTabsDeckComponent.updatePanel();
  selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");
  Assert.ok(selectedPanel.classList.contains("singleDeviceInfo"),
    "tabs fetch panel is selected");

  SyncedTabs._internal.getTabClients.restore();
  sinon.stub(SyncedTabs._internal, "getTabClients", ()=> Promise.resolve([{id: "mock"}]));
  yield syncedTabsDeckComponent.updatePanel();
  selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");
  Assert.ok(selectedPanel.classList.contains("tabs-container"),
    "tabs panel is selected");
});

add_task(testClean);

function checkItem(node, item) {
  Assert.ok(node.classList.contains("item"),
    "Node should have .item class");
  if (item.client) {
    // tab items
    Assert.equal(node.querySelector(".item-title").textContent, item.title,
      "Node's title element's text should match item title");
    Assert.ok(node.classList.contains("tab"),
      "Node should have .tab class");
    Assert.equal(node.dataset.url, item.url,
      "Node's URL should match item URL");
    Assert.equal(node.getAttribute("title"), item.title + "\n" + item.url,
      "Tab node should have correct title attribute");
  } else {
    // client items
    Assert.equal(node.querySelector(".item-title").textContent, item.name,
      "Node's title element's text should match client name");
    Assert.ok(node.classList.contains("client"),
      "Node should have .client class");
    Assert.equal(node.dataset.id, item.id,
      "Node's ID should match item ID");
  }
};

