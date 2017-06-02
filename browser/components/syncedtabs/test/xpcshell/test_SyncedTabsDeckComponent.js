"use strict";

let { SyncedTabs } = Cu.import("resource://services-sync/SyncedTabs.jsm", {});
let { SyncedTabsDeckComponent } = Cu.import("resource:///modules/syncedtabs/SyncedTabsDeckComponent.js", {});
let { TabListComponent } = Cu.import("resource:///modules/syncedtabs/TabListComponent.js", {});
let { SyncedTabsListStore } = Cu.import("resource:///modules/syncedtabs/SyncedTabsListStore.js", {});
let { SyncedTabsDeckStore } = Cu.import("resource:///modules/syncedtabs/SyncedTabsDeckStore.js", {});
let { TabListView } = Cu.import("resource:///modules/syncedtabs/TabListView.js", {});
let { DeckView } = Cu.import("resource:///modules/syncedtabs/SyncedTabsDeckView.js", {});


add_task(async function testInitUninit() {
  let deckStore = new SyncedTabsDeckStore();
  let listComponent = {};
  let mockWindow = {};

  let ViewMock = sinon.stub();
  let view = {render: sinon.spy(), destroy: sinon.spy(), container: {}};
  ViewMock.returns(view);

  sinon.stub(SyncedTabs, "syncTabs", () => Promise.resolve());

  sinon.spy(deckStore, "on");
  sinon.stub(deckStore, "setPanels");

  let component = new SyncedTabsDeckComponent({
    window: mockWindow,
    deckStore,
    listComponent,
    SyncedTabs,
    DeckView: ViewMock,
  });

  sinon.stub(component, "updatePanel");

  component.init();

  Assert.ok(SyncedTabs.syncTabs.called);
  SyncedTabs.syncTabs.restore();

  Assert.ok(ViewMock.calledWithNew(), "view is instantiated");
  Assert.equal(ViewMock.args[0][0], mockWindow);
  Assert.equal(ViewMock.args[0][1], listComponent);
  Assert.ok(ViewMock.args[0][2].onAndroidClick,
    "view is passed onAndroidClick prop");
  Assert.ok(ViewMock.args[0][2].oniOSClick,
    "view is passed oniOSClick prop");
  Assert.ok(ViewMock.args[0][2].onSyncPrefClick,
    "view is passed onSyncPrefClick prop");

  Assert.equal(component.container, view.container,
    "component returns view's container");

  Assert.ok(deckStore.on.calledOnce, "listener is added to store");
  Assert.equal(deckStore.on.args[0][0], "change");
  // Object.values only in nightly
  let values = Object.keys(component.PANELS).map(k => component.PANELS[k]);
  Assert.ok(deckStore.setPanels.calledWith(values),
    "panels are set on deck store");

  Assert.ok(component.updatePanel.called);

  deckStore.emit("change", "mock state");
  Assert.ok(view.render.calledWith("mock state"),
    "view.render is called on state change");

  component.uninit();

  Assert.ok(view.destroy.calledOnce, "view is destroyed on uninit");
});


function waitForObserver() {
  return new Promise((resolve, reject) => {
    Services.obs.addObserver((subject, topic) => {
      resolve();
    }, SyncedTabs.TOPIC_TABS_CHANGED);
  });
}

add_task(async function testObserver() {
  let deckStore = new SyncedTabsDeckStore();
  let listStore = new SyncedTabsListStore(SyncedTabs);
  let listComponent = {};
  let mockWindow = {};

  let ViewMock = sinon.stub();
  let view = {render: sinon.spy(), destroy: sinon.spy(), container: {}};
  ViewMock.returns(view);

  sinon.stub(SyncedTabs, "syncTabs", () => Promise.resolve());

  sinon.spy(deckStore, "on");
  sinon.stub(deckStore, "setPanels");

  sinon.stub(listStore, "getData");

  let component = new SyncedTabsDeckComponent({
    mockWindow,
    deckStore,
    listStore,
    listComponent,
    SyncedTabs,
    DeckView: ViewMock,
  });

  sinon.spy(component, "observe");
  sinon.stub(component, "updatePanel");

  component.init();
  SyncedTabs.syncTabs.restore();
  Assert.ok(component.updatePanel.called, "triggers panel update during init");

  Services.obs.notifyObservers(null, SyncedTabs.TOPIC_TABS_CHANGED);

  Assert.ok(component.observe.calledWith(null, SyncedTabs.TOPIC_TABS_CHANGED),
    "component is notified");

  Assert.ok(listStore.getData.called, "gets list data");
  Assert.ok(component.updatePanel.calledTwice, "triggers panel update");

  Services.obs.notifyObservers(null, FxAccountsCommon.ONLOGIN_NOTIFICATION);

  Assert.ok(component.observe.calledWith(null, FxAccountsCommon.ONLOGIN_NOTIFICATION),
    "component is notified of login");
  Assert.equal(component.updatePanel.callCount, 3, "triggers panel update again");

  Services.obs.notifyObservers(null, "weave:service:login:change");

  Assert.ok(component.observe.calledWith(null, "weave:service:login:change"),
    "component is notified of login change");
  Assert.equal(component.updatePanel.callCount, 4, "triggers panel update again");
});

add_task(async function testPanelStatus() {
  let deckStore = new SyncedTabsDeckStore();
  let listStore = new SyncedTabsListStore();
  let listComponent = {};
  let fxAccounts = {
    accountStatus() {}
  };
  let SyncedTabsMock = {
    getTabClients() {}
  };

  sinon.stub(listStore, "getData");


  let component = new SyncedTabsDeckComponent({
    fxAccounts,
    deckStore,
    listComponent,
    SyncedTabs: SyncedTabsMock
  });

  let isAuthed = false;
  sinon.stub(fxAccounts, "accountStatus", () => Promise.resolve(isAuthed));
  let result = await component.getPanelStatus();
  Assert.equal(result, component.PANELS.NOT_AUTHED_INFO);

  isAuthed = true;

  SyncedTabsMock.loginFailed = true;
  result = await component.getPanelStatus();
  Assert.equal(result, component.PANELS.NOT_AUTHED_INFO);
  SyncedTabsMock.loginFailed = false;

  SyncedTabsMock.isConfiguredToSyncTabs = false;
  result = await component.getPanelStatus();
  Assert.equal(result, component.PANELS.TABS_DISABLED);

  SyncedTabsMock.isConfiguredToSyncTabs = true;

  SyncedTabsMock.hasSyncedThisSession = false;
  result = await component.getPanelStatus();
  Assert.equal(result, component.PANELS.TABS_FETCHING);

  SyncedTabsMock.hasSyncedThisSession = true;

  let clients = [];
  sinon.stub(SyncedTabsMock, "getTabClients", () => Promise.resolve(clients));
  result = await component.getPanelStatus();
  Assert.equal(result, component.PANELS.SINGLE_DEVICE_INFO);

  clients = ["mock-client"];
  result = await component.getPanelStatus();
  Assert.equal(result, component.PANELS.TABS_CONTAINER);

  fxAccounts.accountStatus.restore();
  sinon.stub(fxAccounts, "accountStatus", () => Promise.reject("err"));
  result = await component.getPanelStatus();
  Assert.equal(result, component.PANELS.NOT_AUTHED_INFO);

  sinon.stub(component, "getPanelStatus", () => Promise.resolve("mock-panelId"));
  sinon.spy(deckStore, "selectPanel");
  await component.updatePanel();
  Assert.ok(deckStore.selectPanel.calledWith("mock-panelId"));
});

add_task(async function testActions() {
  let windowMock = {
    openUILink() {},
  };
  let chromeWindowMock = {
    gSync: {
      openPrefs() {}
    }
  };
  sinon.spy(windowMock, "openUILink");
  sinon.spy(chromeWindowMock.gSync, "openPrefs");

  let getChromeWindowMock = sinon.stub();
  getChromeWindowMock.returns(chromeWindowMock);

  let component = new SyncedTabsDeckComponent({
    window: windowMock,
    getChromeWindowMock
  });

  let href = Services.prefs.getCharPref("identity.mobilepromo.android") + "synced-tabs-sidebar";
  component.openAndroidLink("mock-event");
  Assert.ok(windowMock.openUILink.calledWith(href, "mock-event"));

  href = Services.prefs.getCharPref("identity.mobilepromo.ios") + "synced-tabs-sidebar";
  component.openiOSLink("mock-event");
  Assert.ok(windowMock.openUILink.calledWith(href, "mock-event"));

  component.openSyncPrefs();
  Assert.ok(getChromeWindowMock.calledWith(windowMock));
  Assert.ok(chromeWindowMock.gSync.openPrefs.called);
});
