import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {combineReducers, createStore} from "redux";
import {DiscoveryStreamFeed} from "lib/DiscoveryStreamFeed.jsm";
import {reducers} from "common/Reducers.jsm";

const CONFIG_PREF_NAME = "browser.newtabpage.activity-stream.discoverystream.config";
const THIRTY_MINUTES = 30 * 60 * 1000;

describe("DiscoveryStreamFeed", () => {
  let feed;
  let sandbox;
  let configPrefStub;
  let fetchStub;
  let clock;

  beforeEach(() => {
    // Pref
    sandbox = sinon.createSandbox();
    configPrefStub = sandbox.stub(global.Services.prefs, "getStringPref")
      .withArgs(CONFIG_PREF_NAME)
      .returns(JSON.stringify({enabled: false, layout_endpoint: "foo.com"}));

    // Fetch
    fetchStub = sandbox.stub(global, "fetch");

    // Time
    clock = sinon.useFakeTimers();

    // Feed
    feed = new DiscoveryStreamFeed();
    feed.store = createStore(combineReducers(reducers));
  });

  afterEach(() => {
    clock.restore();
    sandbox.restore();
  });

  describe("#observe", () => {
    it("should update state.DiscoveryStream.config when the pref changes", async () => {
      configPrefStub.returns(JSON.stringify({enabled: true, layout_endpoint: "foo"}));

      feed.observe(null, null, CONFIG_PREF_NAME);

      assert.deepEqual(feed.store.getState().DiscoveryStream.config, {enabled: true, layout_endpoint: "foo"});
    });
  });

  describe("#loadLayout", () => {
    it("should fetch data and populate the cache if it is empty", async () => {
      const resp = {layout: ["foo", "bar"]};
      const fakeCache = {};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      fetchStub.resolves({ok: true, json: () => Promise.resolve(resp)});

      await feed.loadLayout();

      assert.calledOnce(fetchStub);
      assert.calledWith(feed.cache.set, "layout", resp);
    });
    it("should fetch data and populate the cache if the cached data is older than 30 mins", async () => {
      const resp = {layout: ["foo", "bar"]};
      const fakeCache = {layout: {layout: ["hello"], _timestamp: Date.now()}};

      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      fetchStub.resolves({ok: true, json: () => Promise.resolve(resp)});

      clock.tick(THIRTY_MINUTES + 1);
      await feed.loadLayout();

      assert.calledOnce(fetchStub);
      assert.calledWith(feed.cache.set, "layout", resp);
    });
    it("should use the cached data and not fetch if the cached data is less than 30 mins old", async () => {
      const fakeCache = {layout: {layout: ["hello"], _timestamp: Date.now()}};

      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      clock.tick(THIRTY_MINUTES - 1);
      await feed.loadLayout();

      assert.notCalled(fetchStub);
      assert.notCalled(feed.cache.set);
    });
    it("should set spocs_endpoint from layout", async () => {
      const resp = {layout: ["foo", "bar"], spocs: {url: "foo.com"}};
      const fakeCache = {};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      fetchStub.resolves({ok: true, json: () => Promise.resolve(resp)});

      await feed.loadLayout();

      assert.equal(feed.store.getState().DiscoveryStream.spocs.spocs_endpoint, "foo.com");
    });
  });

  describe("#loadComponentFeeds", () => {
    it("should populate feeds cache", async () => {
      const fakeComponents = {components: [{feed: {url: "foo.com"}}]};
      const fakeLayout = [fakeComponents, {components: [{}]}, {}];
      const fakeDiscoveryStream = {DiscoveryStream: {layout: fakeLayout}};
      sandbox.stub(feed.store, "getState").returns(fakeDiscoveryStream);
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      const fakeCache = {feeds: {"foo.com": {"lastUpdated": Date.now(), "data": "data"}}};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));

      await feed.loadComponentFeeds();

      assert.calledWith(feed.cache.set, "feeds", {"foo.com": {"data": "data", "lastUpdated": 0}});
    });
  });

  describe("#getComponentFeed", () => {
    it("should fetch fresh data if cache is empty", async () => {
      const fakeCache = {};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed, "fetchComponentFeed").returns(Promise.resolve("data"));

      const feedResp = await feed.getComponentFeed("foo.com");

      assert.equal(feedResp.data, "data");
    });
    it("should fetch fresh data if cache is old", async () => {
      const fakeCache = {feeds: {"foo.com": {lastUpdated: Date.now()}}};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed, "fetchComponentFeed").returns(Promise.resolve("data"));
      clock.tick(THIRTY_MINUTES + 1);

      const feedResp = await feed.getComponentFeed("foo.com");

      assert.equal(feedResp.data, "data");
    });
    it("should return data from cache if it is fresh", async () => {
      const fakeCache = {feeds: {"foo.com": {lastUpdated: Date.now(), data: "data"}}};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed, "fetchComponentFeed").returns(Promise.resolve("old data"));
      clock.tick(THIRTY_MINUTES - 1);

      const feedResp = await feed.getComponentFeed("foo.com");

      assert.equal(feedResp.data, "data");
    });
  });

  describe("#fetchComponentFeed", () => {
    it("should return old feed if fetch failed", async () => {
      fetchStub.resolves({ok: false, json: () => Promise.resolve({})});
      const fakeCache = {feeds: {"foo.com": {lastUpdated: Date.now(), data: "old data"}}};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      clock.tick(THIRTY_MINUTES + 1);

      const feedResp = await feed.getComponentFeed("foo.com");

      assert.equal(feedResp.data, "old data");
    });
    it("should return new feed if fetch succeeds", async () => {
      fetchStub.resolves({ok: true, json: () => Promise.resolve("data")});
      const fakeCache = {feeds: {"foo.com": {lastUpdated: Date.now(), data: "old data"}}};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      clock.tick(THIRTY_MINUTES + 1);

      const feedResp = await feed.getComponentFeed("foo.com");

      assert.equal(feedResp.data, "data");
    });
  });

  describe("#loadSpocs", () => {
    it("should fetch fresh data if cache is empty", async () => {
      sandbox.stub(feed.cache, "get").returns(Promise.resolve());
      sandbox.stub(feed, "fetchSpocs").returns(Promise.resolve("data"));
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      await feed.loadSpocs();

      assert.calledWith(feed.cache.set, "spocs", {"data": "data", "lastUpdated": 0});
      assert.equal(feed.store.getState().DiscoveryStream.spocs.data, "data");
    });
    it("should fetch fresh data if cache is old", async () => {
      const cachedSpoc = {"data": "old", "lastUpdated": Date.now()};
      const cachedData = {"spocs": cachedSpoc};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(cachedData));
      sandbox.stub(feed, "fetchSpocs").returns(Promise.resolve("new"));
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      clock.tick(THIRTY_MINUTES + 1);

      await feed.loadSpocs();

      assert.equal(feed.store.getState().DiscoveryStream.spocs.data, "new");
    });
    it("should return data from cache if it is fresh", async () => {
      const cachedSpoc = {"data": "old", "lastUpdated": Date.now()};
      const cachedData = {"spocs": cachedSpoc};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(cachedData));
      sandbox.stub(feed, "fetchSpocs").returns(Promise.resolve("new"));
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      clock.tick(THIRTY_MINUTES - 1);

      await feed.loadSpocs();

      assert.equal(feed.store.getState().DiscoveryStream.spocs.data, "old");
    });
  });

  describe("#fetchSpocs", () => {
    it("should return old spocs if fetch failed", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      feed.store.dispatch(ac.BroadcastToContent({
        type: at.DISCOVERY_STREAM_SPOCS_ENDPOINT,
        data: "foo.com",
      }));
      fetchStub.resolves({ok: false, json: () => Promise.resolve({})});
      const fakeCache = {spocs: {lastUpdated: Date.now(), data: "old data"}};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      clock.tick(THIRTY_MINUTES + 1);

      await feed.loadSpocs();

      assert.equal(feed.store.getState().DiscoveryStream.spocs.data, "old data");
    });
    it("should return new spocs if fetch succeeds", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      feed.store.dispatch(ac.BroadcastToContent({
        type: at.DISCOVERY_STREAM_SPOCS_ENDPOINT,
        data: "foo.com",
      }));
      fetchStub.resolves({ok: true, json: () => Promise.resolve("new data")});
      const fakeCache = {spocs: {lastUpdated: Date.now(), data: "old data"}};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      clock.tick(THIRTY_MINUTES + 1);

      await feed.loadSpocs();

      assert.equal(feed.store.getState().DiscoveryStream.spocs.data, "new data");
    });
  });

  describe("#clearCache", () => {
    it("should set .layout, .feeds and .spocs to {}", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      await feed.clearCache();

      assert.calledThrice(feed.cache.set);
      const {firstCall} = feed.cache.set;
      const {secondCall} = feed.cache.set;
      const {thirdCall} = feed.cache.set;
      assert.deepEqual(firstCall.args, ["layout", {}]);
      assert.deepEqual(secondCall.args, ["feeds", {}]);
      assert.deepEqual(thirdCall.args, ["spocs", {}]);
    });
  });

  describe("#onAction: INIT", () => {
    it("should be .loaded=false before initialization", () => {
      assert.isFalse(feed.loaded);
    });
    it("should load data, add pref observer, and set .loaded=true if config.enabled is true", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      configPrefStub.returns(JSON.stringify({enabled: true}));
      sandbox.stub(feed, "loadLayout").returns(Promise.resolve());
      sandbox.stub(global.Services.prefs, "addObserver");

      await feed.onAction({type: at.INIT});

      assert.calledOnce(feed.loadLayout);
      assert.calledWith(global.Services.prefs.addObserver, CONFIG_PREF_NAME, feed);
      assert.isTrue(feed.loaded);
    });
  });

  describe("#onAction: DISCOVERY_STREAM_CONFIG_SET_VALUE", () => {
    it("should add the new value to the pref without changing the existing values", async () => {
      sandbox.stub(global.Services.prefs, "setStringPref");
      configPrefStub.returns(JSON.stringify({enabled: true}));

      await feed.onAction({type: at.DISCOVERY_STREAM_CONFIG_SET_VALUE, data: {name: "layout_endpoint", value: "foo.com"}});

      assert.calledWith(global.Services.prefs.setStringPref, CONFIG_PREF_NAME, JSON.stringify({enabled: true, layout_endpoint: "foo.com"}));
    });
  });

  describe("#onAction: DISCOVERY_STREAM_CONFIG_CHANGE", () => {
    it("should call this.loadLayout if config.enabled changes to true ", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      // First initialize
      await feed.onAction({type: at.INIT});
      assert.isFalse(feed.loaded);

      // force clear cached pref value
      feed._prefCache = {};
      configPrefStub.returns(JSON.stringify({enabled: true}));

      sandbox.stub(feed, "clearCache").returns(Promise.resolve());
      sandbox.stub(feed, "loadLayout").returns(Promise.resolve());
      await feed.onAction({type: at.DISCOVERY_STREAM_CONFIG_CHANGE});

      assert.calledOnce(feed.loadLayout);
      assert.calledOnce(feed.clearCache);
      assert.isTrue(feed.loaded);
    });
    it("should clear the cache if a config change happens and config.enabled is true", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      // force clear cached pref value
      feed._prefCache = {};
      configPrefStub.returns(JSON.stringify({enabled: true}));

      sandbox.stub(feed, "clearCache").returns(Promise.resolve());
      await feed.onAction({type: at.DISCOVERY_STREAM_CONFIG_CHANGE});

      assert.calledOnce(feed.clearCache);
    });
    it("should not call this.loadLayout if config.enabled changes to false", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      // force clear cached pref value
      feed._prefCache = {};
      configPrefStub.returns(JSON.stringify({enabled: true}));

      await feed.onAction({type: at.INIT});
      assert.isTrue(feed.loaded);

      feed._prefCache = {};
      configPrefStub.returns(JSON.stringify({enabled: false}));
      sandbox.stub(feed, "clearCache").returns(Promise.resolve());
      sandbox.stub(feed, "loadLayout").returns(Promise.resolve());
      await feed.onAction({type: at.DISCOVERY_STREAM_CONFIG_CHANGE});

      assert.notCalled(feed.loadLayout);
      assert.calledOnce(feed.clearCache);
      assert.isFalse(feed.loaded);
    });
  });
  describe("#onAction: UNINIT", () => {
    it("should remove pref listeners", async () => {
      sandbox.stub(global.Services.prefs, "removeObserver");

      await feed.onAction({type: at.UNINIT});
      assert.calledWith(global.Services.prefs.removeObserver, CONFIG_PREF_NAME, feed);
    });
  });
});
