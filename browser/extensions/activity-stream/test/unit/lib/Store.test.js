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
      store._prefs.set("foo", true);
      store.init(new Map([["foo", () => new Foo()]]));
      store.initFeed("foo");

      assert.isTrue(store.feeds.has("foo"), "foo is set");
      assert.instanceOf(store.feeds.get("foo"), Foo);
    });
    it("should add a .store property to the feed", () => {
      class Foo {}
      store._feedFactories = new Map([["foo", () => new Foo()]]);
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
      store._feedFactories = new Map([["foo", createFeed]]);

      store.initFeed("foo");
      store.uninitFeed("foo");

      assert.calledOnce(feed.uninit);
    });
    it("should remove the feed from .feeds", () => {
      class Foo {}
      store._feedFactories = new Map([["foo", () => new Foo()]]);

      store.initFeed("foo");
      store.uninitFeed("foo");

      assert.isFalse(store.feeds.has("foo"), "foo is not in .feeds");
    });
  });
  describe("onPrefChanged", () => {
    beforeEach(() => {
      sinon.stub(store, "initFeed");
      sinon.stub(store, "uninitFeed");
      store._prefs.set("foo", false);
      store.init(new Map([["foo", () => ({})]]));
    });
    it("should initialize the feed if called with true", () => {
      store.onPrefChanged("foo", true);

      assert.calledWith(store.initFeed, "foo");
      assert.notCalled(store.uninitFeed);
    });
    it("should uninitialize the feed if called with false", () => {
      store.onPrefChanged("foo", false);

      assert.calledWith(store.uninitFeed, "foo");
      assert.notCalled(store.initFeed);
    });
    it("should do nothing if not an expected feed", () => {
      store.onPrefChanged("bar", false);

      assert.notCalled(store.initFeed);
      assert.notCalled(store.uninitFeed);
    });
  });
  describe("#init", () => {
    it("should call .initFeed with each key", () => {
      sinon.stub(store, "initFeed");
      store._prefs.set("foo", true);
      store._prefs.set("bar", true);
      store.init(new Map([["foo", () => {}], ["bar", () => {}]]));
      assert.calledWith(store.initFeed, "foo");
      assert.calledWith(store.initFeed, "bar");
    });
    it("should not initialize the feed if the Pref is set to false", () => {
      sinon.stub(store, "initFeed");
      store._prefs.set("foo", false);
      store.init(new Map([["foo", () => {}]]));
      assert.notCalled(store.initFeed);
    });
    it("should observe the pref branch", () => {
      sinon.stub(store._prefs, "observeBranch");
      store.init(new Map());
      assert.calledOnce(store._prefs.observeBranch);
      assert.calledWith(store._prefs.observeBranch, store);
    });
    it("should initialize the ActivityStreamMessageChannel channel", () => {
      store.init(new Map());
      assert.calledOnce(store._messageChannel.createChannel);
    });
  });
  describe("#uninit", () => {
    it("should clear .feeds and ._feedFactories", () => {
      store._prefs.set("a", true);
      store.init(new Map([
        ["a", () => ({})],
        ["b", () => ({})],
        ["c", () => ({})]
      ]));

      store.uninit();

      assert.equal(store.feeds.size, 0);
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
      store.init(new Map([["sub", () => sub]]));

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
