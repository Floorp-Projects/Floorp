"use strict";

let { SyncedTabs } = Cu.import("resource://services-sync/SyncedTabs.jsm", {});
let { TabListComponent } = Cu.import("resource:///modules/syncedtabs/TabListComponent.js", {});
let { SyncedTabsListStore } = Cu.import("resource:///modules/syncedtabs/SyncedTabsListStore.js", {});
let { View } = Cu.import("resource:///modules/syncedtabs/TabListView.js", {});

const ACTION_METHODS = [
  "onSelectRow",
  "onOpenTab",
  "onMoveSelectionDown",
  "onMoveSelectionUp",
  "onToggleBranch",
  "onBookmarkTab",
  "onSyncRefresh",
  "onFilter",
  "onClearFilter",
  "onFilterFocus",
  "onFilterBlur",
];

add_task(function* testInitUninit() {
  let store = new SyncedTabsListStore();
  let ViewMock = sinon.stub();
  let view = {render(){}, destroy(){}};

  ViewMock.returns(view);

  sinon.spy(view, 'render');
  sinon.spy(view, 'destroy');

  sinon.spy(store, "on");
  sinon.stub(store, "getData");
  sinon.stub(store, "focusInput");

  let component = new TabListComponent({window, store, View: ViewMock, SyncedTabs});

  for (let action of ACTION_METHODS) {
    sinon.stub(component, action);
  }

  component.init();

  Assert.ok(ViewMock.calledWithNew(), "view is instantiated");
  Assert.ok(store.on.calledOnce, "listener is added to store");
  Assert.equal(store.on.args[0][0], "change");
  Assert.ok(view.render.calledWith({clients: []}),
    "render is called on view instance");
  Assert.ok(store.getData.calledOnce, "store gets initial data");
  Assert.ok(store.focusInput.calledOnce, "input field is focused");

  for (let method of ACTION_METHODS) {
    let action = ViewMock.args[0][1][method];
    Assert.ok(action, method + " action is passed to View");
    action("foo", "bar");
    Assert.ok(component[method].calledWith("foo", "bar"),
      method + " action passed to View triggers the component method with args");
  }

  store.emit("change", "mock state");
  Assert.ok(view.render.secondCall.calledWith("mock state"),
    "view.render is called on state change");

  component.uninit();
  Assert.ok(view.destroy.calledOnce, "view is destroyed on uninit");
});

add_task(function* testActions() {
  let store = new SyncedTabsListStore();
  let windowMock = {
    top: {
      PlacesCommandHook: {
        bookmarkLink() { return Promise.resolve(); }
      }
    },
    openUILink() {},
    PlacesUtils: { bookmarksMenuFolderId: "id" },
  };
  let component = new TabListComponent({
    window: windowMock, store, View: null, SyncedTabs});

  sinon.stub(store, "getData");
  component.onFilter("query");
  Assert.ok(store.getData.calledWith("query"));

  sinon.stub(store, "clearFilter");
  component.onClearFilter();
  Assert.ok(store.clearFilter.called);

  sinon.stub(store, "focusInput");
  component.onFilterFocus();
  Assert.ok(store.focusInput.called);

  sinon.stub(store, "blurInput");
  component.onFilterBlur();
  Assert.ok(store.blurInput.called);

  sinon.stub(store, "selectRow");
  sinon.stub(store, "toggleBranch");
  component.onSelectRow([-1, -1], "foo-id");
  Assert.ok(store.selectRow.calledWith(-1, -1));
  Assert.ok(store.toggleBranch.calledWith("foo-id"));

  sinon.stub(store, "moveSelectionDown");
  component.onMoveSelectionDown();
  Assert.ok(store.moveSelectionDown.called);

  sinon.stub(store, "moveSelectionUp");
  component.onMoveSelectionUp();
  Assert.ok(store.moveSelectionUp.called);

  component.onToggleBranch("foo-id");
  Assert.ok(store.toggleBranch.secondCall.calledWith("foo-id"));

  sinon.spy(windowMock.top.PlacesCommandHook, "bookmarkLink");
  component.onBookmarkTab("uri", "title");
  Assert.equal(windowMock.top.PlacesCommandHook.bookmarkLink.args[0][1], "uri");
  Assert.equal(windowMock.top.PlacesCommandHook.bookmarkLink.args[0][2], "title");

  sinon.spy(windowMock, "openUILink");
  component.onOpenTab("uri", "event");
  Assert.ok(windowMock.openUILink.calledWith("uri", "event"));

  sinon.stub(SyncedTabs, "syncTabs");
  component.onSyncRefresh();
  Assert.ok(SyncedTabs.syncTabs.calledWith(true));
  SyncedTabs.syncTabs.restore();
});

