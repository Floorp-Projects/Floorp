"use strict";

let { SyncedTabs } = ChromeUtils.import(
  "resource://services-sync/SyncedTabs.jsm"
);
let { SyncedTabsDeckComponent } = ChromeUtils.import(
  "resource:///modules/syncedtabs/SyncedTabsDeckComponent.js"
);
let { TabListComponent } = ChromeUtils.import(
  "resource:///modules/syncedtabs/TabListComponent.js"
);
let { SyncedTabsListStore } = ChromeUtils.import(
  "resource:///modules/syncedtabs/SyncedTabsListStore.js"
);
let { SyncedTabsDeckStore } = ChromeUtils.import(
  "resource:///modules/syncedtabs/SyncedTabsDeckStore.js"
);
let { TabListView } = ChromeUtils.import(
  "resource:///modules/syncedtabs/TabListView.js"
);
const { UIState } = ChromeUtils.import("resource://services-sync/UIState.jsm");

add_task(async function testInitUninit() {
  let deckStore = new SyncedTabsDeckStore();
  let listComponent = {};
  let mockWindow = {};

  let ViewMock = sinon.stub();
  let view = { render: sinon.spy(), destroy: sinon.spy(), container: {} };
  ViewMock.returns(view);

  sinon.stub(SyncedTabs, "syncTabs").callsFake(() => Promise.resolve());

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
  Assert.ok(
    ViewMock.args[0][2].onConnectDeviceClick,
    "view is passed onConnectDeviceClick prop"
  );
  Assert.ok(
    ViewMock.args[0][2].onSyncPrefClick,
    "view is passed onSyncPrefClick prop"
  );

  Assert.equal(
    component.container,
    view.container,
    "component returns view's container"
  );

  Assert.ok(deckStore.on.calledOnce, "listener is added to store");
  Assert.equal(deckStore.on.args[0][0], "change");
  // Object.values only in nightly
  let values = Object.keys(component.PANELS).map(k => component.PANELS[k]);
  Assert.ok(
    deckStore.setPanels.calledWith(values),
    "panels are set on deck store"
  );

  Assert.ok(component.updatePanel.called);

  deckStore.emit("change", "mock state");
  Assert.ok(
    view.render.calledWith("mock state"),
    "view.render is called on state change"
  );

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
  let view = { render: sinon.spy(), destroy: sinon.spy(), container: {} };
  ViewMock.returns(view);

  sinon.stub(SyncedTabs, "syncTabs").callsFake(() => Promise.resolve());

  sinon.spy(deckStore, "on");
  sinon.stub(deckStore, "setPanels");

  sinon.stub(listStore, "getData");

  let component = new SyncedTabsDeckComponent({
    window: mockWindow,
    deckStore,
    listStore,
    listComponent,
    SyncedTabs,
    DeckView: ViewMock,
  });

  sinon.spy(component, "observe");
  sinon.stub(component, "updatePanel");
  sinon.stub(component, "updateDir");

  component.init();
  SyncedTabs.syncTabs.restore();
  Assert.ok(component.updatePanel.called, "triggers panel update during init");
  Assert.ok(
    component.updateDir.called,
    "triggers UI direction update during init"
  );

  Services.obs.notifyObservers(null, SyncedTabs.TOPIC_TABS_CHANGED);

  Assert.ok(
    component.observe.calledWith(null, SyncedTabs.TOPIC_TABS_CHANGED),
    "component is notified"
  );

  Assert.ok(listStore.getData.called, "gets list data");
  Assert.ok(component.updatePanel.calledTwice, "triggers panel update");

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);

  Assert.ok(
    component.observe.calledWith(null, UIState.ON_UPDATE),
    "component is notified of FxA/Sync UI Update"
  );
  Assert.equal(
    component.updatePanel.callCount,
    3,
    "triggers panel update again"
  );

  Services.locale.availableLocales = ["ab-CD"];
  Services.locale.requestedLocales = ["ab-CD"];

  Assert.ok(
    component.updateDir.calledTwice,
    "locale change triggers UI direction update"
  );

  Services.prefs.setIntPref("intl.uidirection", 1);

  Assert.equal(
    component.updateDir.callCount,
    3,
    "pref change triggers UI direction update"
  );
});

add_task(async function testPanelStatus() {
  let deckStore = new SyncedTabsDeckStore();
  let listStore = new SyncedTabsListStore();
  let listComponent = {};
  let SyncedTabsMock = {
    getTabClients() {},
  };

  sinon.stub(listStore, "getData");

  let component = new SyncedTabsDeckComponent({
    deckStore,
    listComponent,
    SyncedTabs: SyncedTabsMock,
  });

  sinon.stub(UIState, "get").returns({ status: UIState.STATUS_NOT_CONFIGURED });
  let result = await component.getPanelStatus();
  Assert.equal(result, component.PANELS.NOT_AUTHED_INFO);
  UIState.get.restore();

  sinon.stub(UIState, "get").returns({ status: UIState.STATUS_NOT_VERIFIED });
  result = await component.getPanelStatus();
  Assert.equal(result, component.PANELS.UNVERIFIED);
  UIState.get.restore();

  sinon.stub(UIState, "get").returns({ status: UIState.STATUS_LOGIN_FAILED });
  result = await component.getPanelStatus();
  Assert.equal(result, component.PANELS.LOGIN_FAILED);
  UIState.get.restore();

  sinon
    .stub(UIState, "get")
    .returns({ status: UIState.STATUS_SIGNED_IN, syncEnabled: false });
  SyncedTabsMock.isConfiguredToSyncTabs = true;
  result = await component.getPanelStatus();
  Assert.equal(result, component.PANELS.SYNC_DISABLED);
  UIState.get.restore();

  sinon
    .stub(UIState, "get")
    .returns({ status: UIState.STATUS_SIGNED_IN, syncEnabled: true });
  SyncedTabsMock.isConfiguredToSyncTabs = false;
  result = await component.getPanelStatus();
  Assert.equal(result, component.PANELS.TABS_DISABLED);

  SyncedTabsMock.isConfiguredToSyncTabs = true;

  SyncedTabsMock.hasSyncedThisSession = false;
  result = await component.getPanelStatus();
  Assert.equal(result, component.PANELS.TABS_FETCHING);

  SyncedTabsMock.hasSyncedThisSession = true;

  let clients = [];
  sinon
    .stub(SyncedTabsMock, "getTabClients")
    .callsFake(() => Promise.resolve(clients));
  result = await component.getPanelStatus();
  Assert.equal(result, component.PANELS.SINGLE_DEVICE_INFO);

  clients = ["mock-client"];
  result = await component.getPanelStatus();
  Assert.equal(result, component.PANELS.TABS_CONTAINER);

  sinon
    .stub(component, "getPanelStatus")
    .callsFake(() => Promise.resolve("mock-panelId"));
  sinon.spy(deckStore, "selectPanel");
  await component.updatePanel();
  Assert.ok(deckStore.selectPanel.calledWith("mock-panelId"));
});

add_task(async function testActions() {
  let windowMock = {};
  let chromeWindowMock = {
    gSync: {
      openPrefs() {},
      openConnectAnotherDevice() {},
    },
  };
  sinon.spy(chromeWindowMock.gSync, "openPrefs");
  sinon.spy(chromeWindowMock.gSync, "openConnectAnotherDevice");

  let getChromeWindowMock = sinon.stub();
  getChromeWindowMock.returns(chromeWindowMock);

  let component = new SyncedTabsDeckComponent({
    window: windowMock,
    getChromeWindowMock,
  });

  component.openConnectDevice();
  Assert.ok(chromeWindowMock.gSync.openConnectAnotherDevice.called);

  component.openSyncPrefs();
  Assert.ok(getChromeWindowMock.calledWith(windowMock));
  Assert.ok(chromeWindowMock.gSync.openPrefs.called);
});
