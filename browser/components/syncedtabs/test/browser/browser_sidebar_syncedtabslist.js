"use strict";

const FIXTURE = [
  {
    "id": "7cqCr77ptzX3",
    "type": "client",
    "lastModified": 1492201200,
    "name": "zcarter's Nightly on MacBook-Pro-25",
    "isMobile": false,
    "tabs": [
      {
        "type": "tab",
        "title": "Firefox for Android — Mobile Web browser — More ways to customize and protect your privacy — Mozilla",
        "url": "https://www.mozilla.org/en-US/firefox/android/?utm_source=firefox-browser&utm_medium=firefox-browser&utm_campaign=synced-tabs-sidebar",
        "icon": "chrome://mozapps/skin/places/defaultFavicon.svg",
        "client": "7cqCr77ptzX3",
        "lastUsed": 1452124677
      }
    ]
  },
  {
    "id": "2xU5h-4bkWqA",
    "type": "client",
    "lastModified": 1492201200,
    "name": "laptop",
    "isMobile": false,
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
      },
      {
        // Should appear first for this client.
        "type": "tab",
        "title": "Mozilla Developer Network",
        "url": "https://developer.mozilla.org/en-US/",
        "icon": "moz-anno:favicon:https://developer.cdn.mozilla.net/static/img/favicon32.e02854fdcf73.png",
        "client": "2xU5h-4bkWqA",
        "lastUsed": 1451519725
      }
    ]
  },
  {
    "id": "OL3EJCsdb2JD",
    "type": "client",
    "lastModified": 1492201200,
    "name": "desktop",
    "isMobile": false,
    "tabs": []
  }
];

let originalSyncedTabsInternal = null;

async function testClean() {
  let syncedTabsDeckComponent = window.SidebarUI.browser.contentWindow.syncedTabsDeckComponent;
  let SyncedTabs = window.SidebarUI.browser.contentWindow.SyncedTabs;
  syncedTabsDeckComponent._getSignedInUser.restore();
  SyncedTabs._internal.getTabClients.restore();
  SyncedTabs._internal = originalSyncedTabsInternal;

  await new Promise(resolve => {
    window.SidebarUI.browser.contentWindow.addEventListener("unload", function() {
      resolve();
    }, {once: true});
    SidebarUI.hide();
  });
}

add_task(async function testSyncedTabsSidebarList() {
  await SidebarUI.show("viewTabsSidebar");

  Assert.equal(SidebarUI.currentID, "viewTabsSidebar", "Sidebar should have SyncedTabs loaded");

  let syncedTabsDeckComponent = SidebarUI.browser.contentWindow.syncedTabsDeckComponent;
  let SyncedTabs = SidebarUI.browser.contentWindow.SyncedTabs;

  Assert.ok(syncedTabsDeckComponent, "component exists");

  originalSyncedTabsInternal = SyncedTabs._internal;
  SyncedTabs._internal = {
    isConfiguredToSyncTabs: true,
    hasSyncedThisSession: true,
    getTabClients() { return Promise.resolve([]); },
    syncTabs() { return Promise.resolve(); },
  };

  sinon.stub(syncedTabsDeckComponent, "_getSignedInUser", () => Promise.resolve({verified: true}));
  sinon.stub(SyncedTabs._internal, "getTabClients", () => Promise.resolve(Cu.cloneInto(FIXTURE, {})));

  await syncedTabsDeckComponent.updatePanel();
  // This is a hacky way of waiting for the view to render. The view renders
  // after the following promise (a different instance of which is triggered
  // in updatePanel) resolves, so we wait for it here as well
  await syncedTabsDeckComponent.tabListComponent._store.getData();

  Assert.ok(SyncedTabs._internal.getTabClients.called, "get clients called");

  let selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");


  Assert.ok(selectedPanel.classList.contains("tabs-container"),
    "tabs panel is selected");

  Assert.equal(selectedPanel.querySelectorAll(".tab").length, 4,
    "four tabs listed");
  Assert.equal(selectedPanel.querySelectorAll(".client").length, 3,
    "three clients listed");
  Assert.equal(selectedPanel.querySelectorAll(".client")[2].querySelectorAll(".empty").length, 1,
    "third client is empty");

  // Verify that the tabs are sorted by last used time.
  var expectedTabIndices = [[0], [2, 0, 1]];
  Array.prototype.forEach.call(selectedPanel.querySelectorAll(".client"), (clientNode, i) => {
    checkItem(clientNode, FIXTURE[i]);
    Array.prototype.forEach.call(clientNode.querySelectorAll(".tab"), (tabNode, j) => {
      let tabIndex = expectedTabIndices[i][j];
      checkItem(tabNode, FIXTURE[i].tabs[tabIndex]);
    });
  });

});

add_task(testClean);

add_task(async function testSyncedTabsSidebarFilteredList() {
  await SidebarUI.show("viewTabsSidebar");
  let syncedTabsDeckComponent = window.SidebarUI.browser.contentWindow.syncedTabsDeckComponent;
  let SyncedTabs = window.SidebarUI.browser.contentWindow.SyncedTabs;

  Assert.ok(syncedTabsDeckComponent, "component exists");

  originalSyncedTabsInternal = SyncedTabs._internal;
  SyncedTabs._internal = {
    isConfiguredToSyncTabs: true,
    hasSyncedThisSession: true,
    getTabClients() { return Promise.resolve([]); },
    syncTabs() { return Promise.resolve(); },
  };

  sinon.stub(syncedTabsDeckComponent, "_getSignedInUser", () => Promise.resolve({verified: true}));
  sinon.stub(SyncedTabs._internal, "getTabClients", () => Promise.resolve(Cu.cloneInto(FIXTURE, {})));

  await syncedTabsDeckComponent.updatePanel();
  // This is a hacky way of waiting for the view to render. The view renders
  // after the following promise (a different instance of which is triggered
  // in updatePanel) resolves, so we wait for it here as well
  await syncedTabsDeckComponent.tabListComponent._store.getData();

  let filterInput = syncedTabsDeckComponent._window.document.querySelector(".tabsFilter");
  filterInput.value = "filter text";
  filterInput.blur();

  await syncedTabsDeckComponent.tabListComponent._store.getData("filter text");

  let selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");
  Assert.ok(selectedPanel.classList.contains("tabs-container"),
    "tabs panel is selected");

  Assert.equal(selectedPanel.querySelectorAll(".tab").length, 4,
    "four tabs listed");
  Assert.equal(selectedPanel.querySelectorAll(".client").length, 0,
    "no clients are listed");

  Assert.equal(filterInput.value, "filter text",
    "filter text box has correct value");

  // Tabs should not be sorted when filter is active.
  let FIXTURE_TABS = FIXTURE.reduce((prev, client) => prev.concat(client.tabs), []);

  Array.prototype.forEach.call(selectedPanel.querySelectorAll(".tab"), (tabNode, i) => {
    checkItem(tabNode, FIXTURE_TABS[i]);
  });

  // Removing the filter should resort tabs.
  FIXTURE_TABS.sort((a, b) => b.lastUsed - a.lastUsed);
  await syncedTabsDeckComponent.tabListComponent._store.getData();
  Array.prototype.forEach.call(selectedPanel.querySelectorAll(".tab"), (tabNode, i) => {
    checkItem(tabNode, FIXTURE_TABS[i]);
  });
});

add_task(testClean);

add_task(async function testSyncedTabsSidebarStatus() {
  let account = null;

  await SidebarUI.show("viewTabsSidebar");
  let syncedTabsDeckComponent = window.SidebarUI.browser.contentWindow.syncedTabsDeckComponent;
  let SyncedTabs = window.SidebarUI.browser.contentWindow.SyncedTabs;

  originalSyncedTabsInternal = SyncedTabs._internal;
  SyncedTabs._internal = {
    isConfiguredToSyncTabs: false,
    hasSyncedThisSession: false,
    getTabClients() {},
    syncTabs() { return Promise.resolve(); },
  };

  Assert.ok(syncedTabsDeckComponent, "component exists");

  sinon.spy(syncedTabsDeckComponent, "updatePanel");
  sinon.spy(syncedTabsDeckComponent, "observe");

  sinon.stub(syncedTabsDeckComponent, "_getSignedInUser", () => Promise.reject("Test error"));
  await syncedTabsDeckComponent.updatePanel();

  let selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");
  Assert.ok(selectedPanel.classList.contains("notAuthedInfo"),
    "not-authed panel is selected on auth error");

  syncedTabsDeckComponent._getSignedInUser.restore();
  sinon.stub(syncedTabsDeckComponent, "_getSignedInUser", () => Promise.resolve(account));
  await syncedTabsDeckComponent.updatePanel();
  selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");
  Assert.ok(selectedPanel.classList.contains("notAuthedInfo"),
    "not-authed panel is selected");

  account = {verified: false};
  await syncedTabsDeckComponent.updatePanel();
  selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");
  Assert.ok(selectedPanel.classList.contains("unverified"),
    "unverified panel is selected");

  account = {verified: true};
  await syncedTabsDeckComponent.updatePanel();
  selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");
  Assert.ok(selectedPanel.classList.contains("tabs-disabled"),
    "tabs disabled panel is selected");

  SyncedTabs._internal.isConfiguredToSyncTabs = true;
  await syncedTabsDeckComponent.updatePanel();
  selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");
  Assert.ok(selectedPanel.classList.contains("tabs-fetching"),
    "tabs fetch panel is selected");

  SyncedTabs._internal.hasSyncedThisSession = true;
  sinon.stub(SyncedTabs._internal, "getTabClients", () => Promise.resolve([]));
  await syncedTabsDeckComponent.updatePanel();
  selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");
  Assert.ok(selectedPanel.classList.contains("singleDeviceInfo"),
    "tabs fetch panel is selected");

  SyncedTabs._internal.getTabClients.restore();
  sinon.stub(SyncedTabs._internal, "getTabClients", () => Promise.resolve([{id: "mock"}]));
  await syncedTabsDeckComponent.updatePanel();
  selectedPanel = syncedTabsDeckComponent.container.querySelector(".sync-state.selected");
  Assert.ok(selectedPanel.classList.contains("tabs-container"),
    "tabs panel is selected");
});

add_task(testClean);

add_task(async function testSyncedTabsSidebarContextMenu() {
  await SidebarUI.show("viewTabsSidebar");
  let syncedTabsDeckComponent = window.SidebarUI.browser.contentWindow.syncedTabsDeckComponent;
  let SyncedTabs = window.SidebarUI.browser.contentWindow.SyncedTabs;

  Assert.ok(syncedTabsDeckComponent, "component exists");

  originalSyncedTabsInternal = SyncedTabs._internal;
  SyncedTabs._internal = {
    isConfiguredToSyncTabs: true,
    hasSyncedThisSession: true,
    getTabClients() { return Promise.resolve([]); },
    syncTabs() { return Promise.resolve(); },
  };

  sinon.stub(syncedTabsDeckComponent, "_getSignedInUser", () => Promise.resolve({verified: true}));
  sinon.stub(SyncedTabs._internal, "getTabClients", () => Promise.resolve(Cu.cloneInto(FIXTURE, {})));

  await syncedTabsDeckComponent.updatePanel();
  // This is a hacky way of waiting for the view to render. The view renders
  // after the following promise (a different instance of which is triggered
  // in updatePanel) resolves, so we wait for it here as well
  await syncedTabsDeckComponent.tabListComponent._store.getData();

  info("Right-clicking the search box should show text-related actions");
  let filterMenuItems = [
    "menuitem[cmd=cmd_undo]",
    "menuseparator",
    // We don't check whether the commands are enabled due to platform
    // differences. On OS X and Windows, "cut" and "copy" are always enabled
    // for HTML inputs; on Linux, they're only enabled if text is selected.
    "menuitem[cmd=cmd_cut]",
    "menuitem[cmd=cmd_copy]",
    "menuitem[cmd=cmd_paste]",
    "menuitem[cmd=cmd_delete]",
    "menuseparator",
    "menuitem[cmd=cmd_selectAll]",
    "menuseparator",
    "menuitem#syncedTabsRefreshFilter",
  ];
  await testContextMenu(syncedTabsDeckComponent,
                         "#SyncedTabsSidebarTabsFilterContext",
                         ".tabsFilter",
                         filterMenuItems);

  info("Right-clicking a tab should show additional actions");
  let tabMenuItems = [
    ["menuitem#syncedTabsOpenSelected", { hidden: false }],
    ["menuitem#syncedTabsOpenSelectedInTab", { hidden: false }],
    ["menuitem#syncedTabsOpenSelectedInWindow", { hidden: false }],
    ["menuitem#syncedTabsOpenSelectedInPrivateWindow", { hidden: false }],
    ["menuseparator", { hidden: false }],
    ["menuitem#syncedTabsBookmarkSelected", { hidden: false }],
    ["menuitem#syncedTabsCopySelected", { hidden: false }],
    ["menuseparator", { hidden: false }],
    ["menuitem#syncedTabsOpenAllInTabs", { hidden: true }],
    ["menuitem#syncedTabsManageDevices", { hidden: true }],
    ["menuitem#syncedTabsRefresh", { hidden: false }],
  ];
  await testContextMenu(syncedTabsDeckComponent,
                         "#SyncedTabsSidebarContext",
                         "#tab-7cqCr77ptzX3-0",
                         tabMenuItems);

  info("Right-clicking a client should show the Open All in Tabs and Manage devices actions");
  let sidebarMenuItems = [
    ["menuitem#syncedTabsOpenSelected", { hidden: true }],
    ["menuitem#syncedTabsOpenSelectedInTab", { hidden: true }],
    ["menuitem#syncedTabsOpenSelectedInWindow", { hidden: true }],
    ["menuitem#syncedTabsOpenSelectedInPrivateWindow", { hidden: true }],
    ["menuseparator", { hidden: true }],
    ["menuitem#syncedTabsBookmarkSelected", { hidden: true }],
    ["menuitem#syncedTabsCopySelected", { hidden: true }],
    ["menuseparator", { hidden: true }],
    ["menuitem#syncedTabsOpenAllInTabs", { hidden: false }],
    ["menuitem#syncedTabsManageDevices", { hidden: false }],
    ["menuitem#syncedTabsRefresh", { hidden: false }],
  ];
  await testContextMenu(syncedTabsDeckComponent,
                         "#SyncedTabsSidebarContext",
                         "#item-7cqCr77ptzX3",
                         sidebarMenuItems);

  info("Right-clicking a client without any tabs should not show the Open All in Tabs action");
  let menuItems = [
    ["menuitem#syncedTabsOpenSelected", { hidden: true }],
    ["menuitem#syncedTabsOpenSelectedInTab", { hidden: true }],
    ["menuitem#syncedTabsOpenSelectedInWindow", { hidden: true }],
    ["menuitem#syncedTabsOpenSelectedInPrivateWindow", { hidden: true }],
    ["menuseparator", { hidden: true }],
    ["menuitem#syncedTabsBookmarkSelected", { hidden: true }],
    ["menuitem#syncedTabsCopySelected", { hidden: true }],
    ["menuseparator", { hidden: true }],
    ["menuitem#syncedTabsOpenAllInTabs", { hidden: true }],
    ["menuitem#syncedTabsManageDevices", { hidden: false }],
    ["menuitem#syncedTabsRefresh", { hidden: false }],
  ];
  await testContextMenu(syncedTabsDeckComponent,
                         "#SyncedTabsSidebarContext",
                         "#item-OL3EJCsdb2JD",
                         menuItems);
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
}

async function testContextMenu(syncedTabsDeckComponent, contextSelector, triggerSelector, menuSelectors) {
  let contextMenu = document.querySelector(contextSelector);
  let triggerElement = syncedTabsDeckComponent._window.document.querySelector(triggerSelector);
  let isClosed = triggerElement.classList.contains("closed");

  let promisePopupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");

  let chromeWindow = triggerElement.ownerGlobal.top;
  let rect = triggerElement.getBoundingClientRect();
  let contentRect = chromeWindow.SidebarUI.browser.getBoundingClientRect();
  // The offsets in `rect` are relative to the content window, but
  // `synthesizeMouseAtPoint` calls `nsIDOMWindowUtils.sendMouseEvent`,
  // which interprets the offsets relative to the containing *chrome* window.
  // This means we need to account for the width and height of any elements
  // outside the `browser` element, like `sidebarheader`.
  let offsetX = contentRect.x + rect.x + (rect.width / 2);
  let offsetY = contentRect.y + rect.y + (rect.height / 4);

  await EventUtils.synthesizeMouseAtPoint(offsetX, offsetY, {
    type: "contextmenu",
    button: 2,
  }, chromeWindow);
  await promisePopupShown;
  is(triggerElement.classList.contains("closed"), isClosed,
    "Showing the context menu shouldn't toggle the tab list");
  checkChildren(contextMenu, menuSelectors);

  let promisePopupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  contextMenu.hidePopup();
  await promisePopupHidden;
}

function checkChildren(node, selectors) {
  is(node.children.length, selectors.length, "Menu item count doesn't match");
  for (let index = 0; index < node.children.length; index++) {
    let child = node.children[index];
    let [selector, props] = [].concat(selectors[index]);
    ok(selector, `Node at ${index} should have selector`);
    ok(child.matches(selector), `Node ${
      index} should match ${selector}`);
    if (props) {
      Object.keys(props).forEach(prop => {
        is(child[prop], props[prop], `${prop} value at ${index} should match`);
      });
    }
  }
}
