"use strict";

let { SyncedTabs } = Cu.import("resource://services-sync/SyncedTabs.jsm", {});
let { SyncedTabsListStore } = Cu.import("resource:///modules/syncedtabs/SyncedTabsListStore.js", {});

const FIXTURE = [
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

add_task(function* testGetDataEmpty() {
  let store = new SyncedTabsListStore(SyncedTabs);
  let spy = sinon.spy();

  sinon.stub(SyncedTabs, "getTabClients", () => {
    return Promise.resolve([]);
  });
  store.on("change", spy);

  yield store.getData();

  Assert.ok(SyncedTabs.getTabClients.calledWith(""));
  Assert.ok(spy.calledWith({
    clients: [],
    canUpdateAll: false,
    canUpdateInput: false,
    filter: "",
    inputFocused: false
  }));

  yield store.getData("filter");

  Assert.ok(SyncedTabs.getTabClients.calledWith("filter"));
  Assert.ok(spy.calledWith({
    clients: [],
    canUpdateAll: false,
    canUpdateInput: true,
    filter: "filter",
    inputFocused: false
  }));

  SyncedTabs.getTabClients.restore();
});

add_task(function* testRowSelectionWithoutFilter() {
  let store = new SyncedTabsListStore(SyncedTabs);
  let spy = sinon.spy();

  sinon.stub(SyncedTabs, "getTabClients", () => {
    return Promise.resolve(FIXTURE);
  });

  yield store.getData();
  SyncedTabs.getTabClients.restore();

  store.on("change", spy);

  store.selectRow(0, -1);
  Assert.ok(spy.args[0][0].canUpdateAll, "can update the whole view");
  Assert.ok(spy.args[0][0].clients[0].selected, "first client is selected");

  store.moveSelectionUp();
  Assert.ok(spy.calledOnce,
    "can't move up past first client, no change triggered");

  store.selectRow(0, 0);
  Assert.ok(spy.args[1][0].clients[0].tabs[0].selected,
    "first tab of first client is selected");

  store.selectRow(0, 0);
  Assert.ok(spy.calledTwice, "selecting same row doesn't trigger change");

  store.selectRow(0, 1);
  Assert.ok(spy.args[2][0].clients[0].tabs[1].selected,
    "second tab of first client is selected");

  store.selectRow(1);
  Assert.ok(spy.args[3][0].clients[1].selected, "second client is selected");

  store.moveSelectionDown();
  Assert.equal(spy.callCount, 4,
    "can't move selection down past last client, no change triggered");

  store.moveSelectionUp();
  Assert.equal(spy.callCount, 5,
    "changed");
  Assert.ok(spy.args[4][0].clients[0].tabs[FIXTURE[0].tabs.length - 1].selected,
    "move selection up from client selects last tab of previous client");

  store.moveSelectionUp();
  Assert.ok(spy.args[5][0].clients[0].tabs[FIXTURE[0].tabs.length - 2].selected,
    "move selection up from tab selects previous tab of client");
});


add_task(function* testToggleBranches() {
  let store = new SyncedTabsListStore(SyncedTabs);
  let spy = sinon.spy();

  sinon.stub(SyncedTabs, "getTabClients", () => {
    return Promise.resolve(FIXTURE);
  });

  yield store.getData();
  SyncedTabs.getTabClients.restore();

  store.selectRow(0);
  store.on("change", spy);

  let clientId = FIXTURE[0].id;
  store.closeBranch(clientId);
  Assert.ok(spy.args[0][0].clients[0].closed, "first client is closed");

  store.openBranch(clientId);
  Assert.ok(!spy.args[1][0].clients[0].closed, "first client is open");

  store.toggleBranch(clientId);
  Assert.ok(spy.args[2][0].clients[0].closed, "first client is toggled closed");

  store.moveSelectionDown();
  Assert.ok(spy.args[3][0].clients[1].selected,
    "selection skips tabs if client is closed");

  store.moveSelectionUp();
  Assert.ok(spy.args[4][0].clients[0].selected,
    "selection skips tabs if client is closed");
});


add_task(function* testRowSelectionWithFilter() {
  let store = new SyncedTabsListStore(SyncedTabs);
  let spy = sinon.spy();

  sinon.stub(SyncedTabs, "getTabClients", () => {
    return Promise.resolve(FIXTURE);
  });

  yield store.getData("filter");
  SyncedTabs.getTabClients.restore();

  store.on("change", spy);

  store.selectRow(0);
  Assert.ok(spy.args[0][0].clients[0].tabs[0].selected, "first tab is selected");

  store.moveSelectionUp();
  Assert.ok(spy.calledOnce,
    "can't move up past first tab, no change triggered");

  store.moveSelectionDown();
  Assert.ok(spy.args[1][0].clients[0].tabs[1].selected,
    "selection skips tabs if client is closed");

  store.moveSelectionDown();
  Assert.equal(spy.callCount, 2,
    "can't move selection down past last tab, no change triggered");

  store.selectRow(1);
  Assert.equal(spy.callCount, 2,
    "doesn't trigger change if same row selected");

});


add_task(function* testFilterAndClearFilter() {
  let store = new SyncedTabsListStore(SyncedTabs);
  let spy = sinon.spy();

  sinon.stub(SyncedTabs, "getTabClients", () => {
    return Promise.resolve(FIXTURE);
  });
  store.on("change", spy);

  yield store.getData("filter");

  Assert.ok(SyncedTabs.getTabClients.calledWith("filter"));
  Assert.ok(!spy.args[0][0].canUpdateAll, "can't update all");
  Assert.ok(spy.args[0][0].canUpdateInput, "can update just input");

  store.selectRow(0);

  Assert.equal(spy.args[1][0].filter, "filter");
  Assert.ok(spy.args[1][0].clients[0].tabs[0].selected,
    "tab is selected");

  yield store.clearFilter();

  Assert.ok(SyncedTabs.getTabClients.calledWith(""));
  Assert.ok(!spy.args[2][0].canUpdateAll, "can't update all");
  Assert.ok(!spy.args[2][0].canUpdateInput, "can't just update input");

  Assert.equal(spy.args[2][0].filter, "");
  Assert.ok(!spy.args[2][0].clients[0].tabs[0].selected,
    "tab is no longer selected");

  SyncedTabs.getTabClients.restore();
});

add_task(function* testFocusBlurInput() {
  let store = new SyncedTabsListStore(SyncedTabs);
  let spy = sinon.spy();

  sinon.stub(SyncedTabs, "getTabClients", () => {
    return Promise.resolve(FIXTURE);
  });
  store.on("change", spy);

  yield store.getData();
  SyncedTabs.getTabClients.restore();

  Assert.ok(!spy.args[0][0].canUpdateAll, "must rerender all");

  store.selectRow(0);
  Assert.ok(!spy.args[1][0].inputFocused,
    "input is not focused");
  Assert.ok(spy.args[1][0].clients[0].selected,
    "client is selected");
  Assert.ok(spy.args[1][0].clients[0].focused,
    "client is focused");

  store.focusInput();
  Assert.ok(spy.args[2][0].inputFocused,
    "input is focused");
  Assert.ok(spy.args[2][0].clients[0].selected,
    "client is still selected");
  Assert.ok(!spy.args[2][0].clients[0].focused,
    "client is no longer focused");

  store.blurInput();
  Assert.ok(!spy.args[3][0].inputFocused,
    "input is not focused");
  Assert.ok(spy.args[3][0].clients[0].selected,
    "client is selected");
  Assert.ok(spy.args[3][0].clients[0].focused,
    "client is focused");
});

