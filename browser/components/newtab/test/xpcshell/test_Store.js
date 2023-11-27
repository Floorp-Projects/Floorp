/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Store } = ChromeUtils.import(
  "resource://activity-stream/lib/Store.jsm"
);
const { ActivityStreamMessageChannel } = ChromeUtils.import(
  "resource://activity-stream/lib/ActivityStreamMessageChannel.jsm"
);
const { ActivityStreamStorage } = ChromeUtils.import(
  "resource://activity-stream/lib/ActivityStreamStorage.jsm"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

// This creates the Redux top-level object.
/* globals Redux */
Services.scriptloader.loadSubScript(
  "resource://activity-stream/vendor/redux.js",
  this
);

add_task(async function test_expected_properties() {
  let sandbox = sinon.createSandbox();
  let store = new Store();

  Assert.equal(store.feeds.constructor.name, "Map", "Should create a Map");
  Assert.equal(store.feeds.size, 0, "Store should start without any feeds.");

  Assert.ok(store._store, "Has a ._store");
  Assert.ok(store.dispatch, "Has a .dispatch");
  Assert.ok(store.getState, "Has a .getState");

  sandbox.restore();
});

add_task(async function test_messagechannel() {
  let sandbox = sinon.createSandbox();
  sandbox
    .stub(ActivityStreamMessageChannel.prototype, "middleware")
    .returns(s => next => action => next(action));
  let store = new Store();

  info(
    "Store should create a ActivityStreamMessageChannel with the right dispatcher"
  );
  Assert.ok(store.getMessageChannel(), "Has a message channel");
  Assert.equal(
    store.getMessageChannel().dispatch,
    store.dispatch,
    "MessageChannel.dispatch forwards to store.dispatch"
  );
  Assert.equal(
    store.getMessageChannel(),
    store._messageChannel,
    "_messageChannel is the member for getMessageChannel()"
  );

  store.dispatch({ type: "FOO" });
  Assert.ok(
    ActivityStreamMessageChannel.prototype.middleware.calledOnce,
    "Middleware called."
  );
  sandbox.restore();
});

add_task(async function test_initFeed_add_feeds() {
  info("Store.initFeed should add an instance of the feed to .feeds");

  let sandbox = sinon.createSandbox();
  let store = new Store();
  class Foo {}
  store._prefs.set("foo", true);
  await store.init(new Map([["foo", () => new Foo()]]));
  store.initFeed("foo");

  Assert.ok(store.feeds.has("foo"), "foo is set");
  Assert.ok(store.feeds.get("foo") instanceof Foo, "Got registered class");
  sandbox.restore();
});

add_task(async function test_initFeed_calls_onAction() {
  info("Store should call the feed's onAction with uninit action if it exists");

  let sandbox = sinon.createSandbox();
  let store = new Store();
  let testFeed;
  let createTestFeed = () => {
    testFeed = { onAction: sandbox.spy() };
    return testFeed;
  };
  const action = { type: "FOO" };
  store._feedFactories = new Map([["test", createTestFeed]]);

  store.initFeed("test", action);

  Assert.ok(testFeed.onAction.calledOnce, "onAction called");
  Assert.ok(
    testFeed.onAction.calledWith(action),
    "onAction called with test action"
  );

  info("Store should add a .store property to the feed");
  Assert.ok(testFeed.store, "Store exists");
  Assert.equal(testFeed.store, store, "Feed store is the Store");
  sandbox.restore();
});

add_task(async function test_initFeed_on_init() {
  info("Store should call .initFeed with each key");

  let sandbox = sinon.createSandbox();
  let store = new Store();

  sandbox.stub(store, "initFeed");
  store._prefs.set("foo", true);
  store._prefs.set("bar", true);
  await store.init(
    new Map([
      ["foo", () => {}],
      ["bar", () => {}],
    ])
  );
  Assert.ok(store.initFeed.calledWith("foo"), "First test feed initted");
  Assert.ok(store.initFeed.calledWith("bar"), "Second test feed initted");
  sandbox.restore();
});

add_task(async function test_initFeed_calls__initIndexedDB() {
  info("Store should call _initIndexedDB");
  let sandbox = sinon.createSandbox();
  let store = new Store();

  sandbox.spy(store, "_initIndexedDB");

  let dbStub = sandbox.stub(ActivityStreamStorage.prototype, "db");
  let dbAccessed = false;
  dbStub.get(() => {
    dbAccessed = true;
    return {};
  });

  store._prefs.set("testfeed", true);
  await store.init(
    new Map([
      [
        "testfeed",
        () => {
          return {};
        },
      ],
    ])
  );

  Assert.ok(store._initIndexedDB.calledOnce, "_initIndexedDB called once");
  Assert.ok(
    store._initIndexedDB.calledWithExactly("feeds.telemetry"),
    "feeds.telemetry was passed"
  );
  // Due to what appears to be a bug in sinon when using calledOnce
  // with a stubbed getter, we can't just use dbStub.calledOnce here.
  Assert.ok(dbAccessed, "ActivityStreamStorage was accessed");

  info(
    "Store should reset ActivityStreamStorage telemetry if opening the db fails"
  );
  dbStub.rejects();
  await store.init(new Map());

  Assert.equal(
    store.dbStorage.telemetry,
    null,
    "Telemetry on storage was cleared"
  );

  sandbox.restore();
});

add_task(async function test_disabled_feed() {
  info("Store should not initialize the feed if the Pref is set to false");

  let sandbox = sinon.createSandbox();
  let store = new Store();

  sandbox.stub(store, "initFeed");
  store._prefs.set("foo", false);
  await store.init(new Map([["foo", () => {}]]));
  Assert.ok(store.initFeed.notCalled, ".initFeed not called");

  store._prefs.set("foo", true);

  sandbox.restore();
});

add_task(async function test_observe_pref_branch() {
  info("Store should observe the pref branch");

  let sandbox = sinon.createSandbox();
  let store = new Store();

  sandbox.stub(store._prefs, "observeBranch");
  await store.init(new Map());
  Assert.ok(store._prefs.observeBranch.calledOnce, "observeBranch called once");
  Assert.ok(
    store._prefs.observeBranch.calledWith(store),
    "observeBranch passed the store"
  );

  sandbox.restore();
});

add_task(async function test_emit_initial_event() {
  info("Store should emit an initial event if provided");

  let sandbox = sinon.createSandbox();
  let store = new Store();

  const action = { type: "FOO" };
  sandbox.stub(store, "dispatch");
  await store.init(new Map(), action);
  Assert.ok(store.dispatch.calledOnce, "Dispatch called once");
  Assert.ok(store.dispatch.calledWith(action), "Dispatch called with action");

  sandbox.restore();
});

add_task(async function test_initialize_telemetry_feed_first() {
  info("Store should initialize the telemetry feed first");

  let sandbox = sinon.createSandbox();
  let store = new Store();

  store._prefs.set("feeds.foo", true);
  store._prefs.set("feeds.telemetry", true);
  const telemetrySpy = sandbox.stub().returns({});
  const fooSpy = sandbox.stub().returns({});
  // Intentionally put the telemetry feed as the second item.
  const feedFactories = new Map([
    ["feeds.foo", fooSpy],
    ["feeds.telemetry", telemetrySpy],
  ]);
  await store.init(feedFactories);
  Assert.ok(telemetrySpy.calledBefore(fooSpy), "Telemetry feed initted first");

  sandbox.restore();
});

add_task(async function test_dispatch_init_load_events() {
  info("Store should dispatch init/load events");

  let sandbox = sinon.createSandbox();
  let store = new Store();

  sandbox.stub(store.getMessageChannel(), "simulateMessagesForExistingTabs");
  await store.init(new Map(), { type: "FOO" });
  Assert.ok(
    store.getMessageChannel().simulateMessagesForExistingTabs.calledOnce,
    "simulateMessagesForExistingTabs called once"
  );

  sandbox.restore();
});

add_task(async function test_init_before_load() {
  info("Store should dispatch INIT before LOAD");

  let sandbox = sinon.createSandbox();
  let store = new Store();

  sandbox.stub(store.getMessageChannel(), "simulateMessagesForExistingTabs");
  sandbox.stub(store, "dispatch");
  const init = { type: "INIT" };
  const load = { type: "TAB_LOAD" };
  store
    .getMessageChannel()
    .simulateMessagesForExistingTabs.callsFake(() => store.dispatch(load));
  await store.init(new Map(), init);

  Assert.ok(store.dispatch.calledTwice, "Dispatch called twice");
  Assert.equal(
    store.dispatch.firstCall.args[0],
    init,
    "First dispatch was for init event"
  );
  Assert.equal(
    store.dispatch.secondCall.args[0],
    load,
    "Second dispatch was for load event"
  );

  sandbox.restore();
});

add_task(async function test_uninit_feeds() {
  info("uninitFeed should not throw if no feed with that name exists");

  let sandbox = sinon.createSandbox();
  let store = new Store();

  try {
    store.uninitFeed("does-not-exist");
    Assert.ok(true, "Didn't throw");
  } catch (e) {
    Assert.ok(false, "Should not have thrown");
  }

  info(
    "uninitFeed should call the feed's onAction with uninit action if it exists"
  );
  let feed;
  function createFeed() {
    feed = { onAction: sandbox.spy() };
    return feed;
  }
  const action = { type: "BAR" };
  store._feedFactories = new Map([["foo", createFeed]]);
  store.initFeed("foo");

  store.uninitFeed("foo", action);

  Assert.ok(feed.onAction.calledOnce);
  Assert.ok(feed.onAction.calledWith(action));

  info("uninitFeed should remove the feed from .feeds");
  Assert.ok(!store.feeds.has("foo"), "foo is not in .feeds");

  sandbox.restore();
});

add_task(async function test_onPrefChanged() {
  let sandbox = sinon.createSandbox();
  let store = new Store();
  let initFeedStub = sandbox.stub(store, "initFeed");
  let uninitFeedStub = sandbox.stub(store, "uninitFeed");
  store._prefs.set("foo", false);
  store.init(new Map([["foo", () => ({})]]));

  info("onPrefChanged should initialize the feed if called with true");
  store.onPrefChanged("foo", true);
  Assert.ok(initFeedStub.calledWith("foo"));
  Assert.ok(!uninitFeedStub.calledOnce);
  initFeedStub.resetHistory();
  uninitFeedStub.resetHistory();

  info("onPrefChanged should uninitialize the feed if called with false");
  store.onPrefChanged("foo", false);
  Assert.ok(uninitFeedStub.calledWith("foo"));
  Assert.ok(!initFeedStub.calledOnce);
  initFeedStub.resetHistory();
  uninitFeedStub.resetHistory();

  info("onPrefChanged should do nothing if not an expected feed");
  store.onPrefChanged("bar", false);

  Assert.ok(!initFeedStub.calledOnce);
  Assert.ok(!uninitFeedStub.calledOnce);
  sandbox.restore();
});

add_task(async function test_uninit() {
  let sandbox = sinon.createSandbox();
  let store = new Store();
  let dispatchStub = sandbox.stub(store, "dispatch");
  const action = { type: "BAR" };
  await store.init(new Map(), null, action);
  store.uninit();

  Assert.ok(store.dispatch.calledOnce);
  Assert.ok(store.dispatch.calledWith(action));

  info("Store.uninit should clear .feeds and ._feedFactories");
  store._prefs.set("a", true);
  await store.init(
    new Map([
      ["a", () => ({})],
      ["b", () => ({})],
      ["c", () => ({})],
    ])
  );

  store.uninit();

  Assert.equal(store.feeds.size, 0);
  Assert.equal(store._feedFactories, null);

  info("Store.uninit should emit an uninit event if provided on init");
  dispatchStub.resetHistory();
  const uninitAction = { type: "BAR" };
  await store.init(new Map(), null, uninitAction);
  store.uninit();

  Assert.ok(store.dispatch.calledOnce);
  Assert.ok(store.dispatch.calledWith(uninitAction));
  sandbox.restore();
});

add_task(async function test_getState() {
  info("Store.getState should return the redux state");
  let sandbox = sinon.createSandbox();
  let store = new Store();
  store._store = Redux.createStore((prevState = 123) => prevState);
  const { getState } = store;
  Assert.equal(getState(), 123);
  sandbox.restore();
});

/**
 * addNumberReducer - a simple dummy reducer for testing that adds a number
 */
function addNumberReducer(prevState = 0, action) {
  return action.type === "ADD" ? prevState + action.data : prevState;
}

add_task(async function test_dispatch() {
  info("Store.dispatch should call .onAction of each feed");
  let sandbox = sinon.createSandbox();
  let store = new Store();
  const { dispatch } = store;
  const sub = { onAction: sinon.spy() };
  const action = { type: "FOO" };

  store._prefs.set("sub", true);
  await store.init(new Map([["sub", () => sub]]));

  dispatch(action);

  Assert.ok(sub.onAction.calledWith(action));

  info("Sandbox.dispatch should call the reducers");

  store._store = Redux.createStore(addNumberReducer);
  dispatch({ type: "ADD", data: 14 });
  Assert.equal(store.getState(), 14);

  sandbox.restore();
});

add_task(async function test_subscribe() {
  info("Store.subscribe should subscribe to changes to the store");
  let sandbox = sinon.createSandbox();
  let store = new Store();
  const sub = sandbox.spy();
  const action = { type: "FOO" };

  store.subscribe(sub);
  store.dispatch(action);

  Assert.ok(sub.calledOnce);
  sandbox.restore();
});
