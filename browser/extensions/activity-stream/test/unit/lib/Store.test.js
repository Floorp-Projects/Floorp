const injector = require("inject!lib/Store.jsm");
const {createStore} = require("redux");
const {addNumberReducer} = require("test/unit/utils");
const {FakePrefs} = require("test/unit/utils");
describe("Store", () => {
  let Store;
  let sandbox;
  let store;
  beforeEach(() => {
    sandbox = sinon.sandbox.create();
    function ActivityStreamMessageChannel(options) {
      this.dispatch = options.dispatch;
      this.createChannel = sandbox.spy();
      this.destroyChannel = sandbox.spy();
      this.middleware = sandbox.spy(s => next => action => next(action));
    }
    ({Store} = injector({
      "lib/ActivityStreamMessageChannel.jsm": {ActivityStreamMessageChannel},
      "lib/ActivityStreamPrefs.jsm": {Prefs: FakePrefs}
    }));
    store = new Store();
  });
  afterEach(() => {
    sandbox.restore();
  });
  it("should have a .feeds property that is a Map", () => {
    assert.instanceOf(store.feeds, Map);
    assert.equal(store.feeds.size, 0, ".feeds.size");
  });
  it("should have a redux store at ._store", () => {
    assert.ok(store._store);
    assert.property(store, "dispatch");
    assert.property(store, "getState");
  });
  it("should create a ActivityStreamMessageChannel with the right dispatcher", () => {
    assert.ok(store._messageChannel);
    assert.equal(store._messageChannel.dispatch, store.dispatch);
  });
  it("should connect the ActivityStreamMessageChannel's middleware", () => {
    store.dispatch({type: "FOO"});
    assert.calledOnce(store._messageChannel.middleware);
  });
  describe("#initFeed", () => {
    it("should add an instance of the feed to .feeds", () => {
      class Foo {}
      store._prefs.set("foo", false);
      store.init({foo: () => new Foo()});
      store.initFeed("foo");

      assert.isTrue(store.feeds.has("foo"), "foo is set");
      assert.instanceOf(store.feeds.get("foo"), Foo);
    });
    it("should add a .store property to the feed", () => {
      class Foo {}
      store._feedFactories = {foo: () => new Foo()};
      store.initFeed("foo");

      assert.propertyVal(store.feeds.get("foo"), "store", store);
    });
  });
  describe("#uninitFeed", () => {
    it("should not throw if no feed with that name exists", () => {
      assert.doesNotThrow(() => {
        store.uninitFeed("bar");
      });
    });
    it("should call the feed's uninit function if it is defined", () => {
      let feed;
      function createFeed() {
        feed = {uninit: sinon.spy()};
        return feed;
      }
      store._feedFactories = {foo: createFeed};

      store.initFeed("foo");
      store.uninitFeed("foo");

      assert.calledOnce(feed.uninit);
    });
    it("should remove the feed from .feeds", () => {
      class Foo {}
      store._feedFactories = {foo: () => new Foo()};

      store.initFeed("foo");
      store.uninitFeed("foo");

      assert.isFalse(store.feeds.has("foo"), "foo is not in .feeds");
    });
  });
  describe("maybeStartFeedAndListenForPrefChanges", () => {
    beforeEach(() => {
      sinon.stub(store, "initFeed");
      sinon.stub(store, "uninitFeed");
    });
    it("should initialize the feed if the Pref is set to true", () => {
      store._prefs.set("foo", true);
      store.maybeStartFeedAndListenForPrefChanges("foo");
      assert.calledWith(store.initFeed, "foo");
    });
    it("should not initialize the feed if the Pref is set to false", () => {
      store._prefs.set("foo", false);
      store.maybeStartFeedAndListenForPrefChanges("foo");
      assert.notCalled(store.initFeed);
    });
    it("should observe the pref", () => {
      sinon.stub(store._prefs, "observe");
      store.maybeStartFeedAndListenForPrefChanges("foo");
      assert.calledWith(store._prefs.observe, "foo", store._prefHandlers.get("foo"));
    });
    describe("handler", () => {
      let handler;
      beforeEach(() => {
        store.maybeStartFeedAndListenForPrefChanges("foo");
        handler = store._prefHandlers.get("foo");
      });
      it("should initialize the feed if called with true", () => {
        handler(true);
        assert.calledWith(store.initFeed, "foo");
      });
      it("should uninitialize the feed if called with false", () => {
        handler(false);
        assert.calledWith(store.uninitFeed, "foo");
      });
    });
  });
  describe("#init", () => {
    it("should call .maybeStartFeedAndListenForPrefChanges with each key", () => {
      sinon.stub(store, "maybeStartFeedAndListenForPrefChanges");
      store.init({foo: () => {}, bar: () => {}});
      assert.calledWith(store.maybeStartFeedAndListenForPrefChanges, "foo");
      assert.calledWith(store.maybeStartFeedAndListenForPrefChanges, "bar");
    });
    it("should initialize the ActivityStreamMessageChannel channel", () => {
      store.init();
      assert.calledOnce(store._messageChannel.createChannel);
    });
  });
  describe("#uninit", () => {
    it("should clear .feeds, ._prefHandlers, and ._feedFactories", () => {
      store._prefs.set("a", true);
      store._prefs.set("b", true);
      store._prefs.set("c", true);
      store.init({
        a: () => ({}),
        b: () => ({}),
        c: () => ({})
      });

      store.uninit();

      assert.equal(store.feeds.size, 0);
      assert.equal(store._prefHandlers.size, 0);
      assert.isNull(store._feedFactories);
    });
    it("should destroy the ActivityStreamMessageChannel channel", () => {
      store.uninit();
      assert.calledOnce(store._messageChannel.destroyChannel);
    });
  });
  describe("#getState", () => {
    it("should return the redux state", () => {
      store._store = createStore((prevState = 123) => prevState);
      const {getState} = store;
      assert.equal(getState(), 123);
    });
  });
  describe("#dispatch", () => {
    it("should call .onAction of each feed", () => {
      const {dispatch} = store;
      const sub = {onAction: sinon.spy()};
      const action = {type: "FOO"};

      store._prefs.set("sub", true);
      store.init({sub: () => sub});

      dispatch(action);

      assert.calledWith(sub.onAction, action);
    });
    it("should call the reducers", () => {
      const {dispatch} = store;
      store._store = createStore(addNumberReducer);

      dispatch({type: "ADD", data: 14});

      assert.equal(store.getState(), 14);
    });
  });
  describe("#subscribe", () => {
    it("should subscribe to changes to the store", () => {
      const sub = sinon.spy();
      const action = {type: "FOO"};

      store.subscribe(sub);
      store.dispatch(action);

      assert.calledOnce(sub);
    });
  });
});
