import {combineReducers, createStore} from "redux";
import {actionTypes as at} from "common/Actions.jsm";
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

  describe("#loadCachedData", () => {
    it("should fetch data and populate the cache if it is empty", async () => {
      const resp = {layout: ["foo", "bar"]};
      const fakeCache = {};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      fetchStub.resolves({ok: true, json: () => Promise.resolve(resp)});

      await feed.loadCachedData();

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
      await feed.loadCachedData();

      assert.calledOnce(fetchStub);
      assert.calledWith(feed.cache.set, "layout", resp);
    });
    it("should use the cached data and not fetch if the cached data is less than 30 mins old", async () => {
      const fakeCache = {layout: {layout: ["hello"], _timestamp: Date.now()}};

      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      clock.tick(THIRTY_MINUTES - 1);
      await feed.loadCachedData();

      assert.notCalled(fetchStub);
      assert.notCalled(feed.cache.set);
    });
  });

  describe("#clearCache", () => {
    it("should set .layout to {}", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      await feed.clearCache();

      assert.calledOnce(feed.cache.set);
      assert.calledWith(feed.cache.set, "layout", {});
    });
  });

  describe("#onAction: INIT", () => {
    it("should be .loaded=false before initialization", () => {
      assert.isFalse(feed.loaded);
    });
    it("should load data, add pref observer, and set .loaded=true if config.enabled is true", async () => {
      configPrefStub.returns(JSON.stringify({enabled: true}));
      sandbox.stub(feed, "loadCachedData").returns(Promise.resolve());
      sandbox.stub(global.Services.prefs, "addObserver");

      await feed.onAction({type: at.INIT});

      assert.calledOnce(feed.loadCachedData);
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
    it("should call this.loadCachedData if config.enabled changes to true ", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      // First initialize
      await feed.onAction({type: at.INIT});
      assert.isFalse(feed.loaded);

      // force clear cached pref value
      feed._prefCache = {};
      configPrefStub.returns(JSON.stringify({enabled: true}));

      sandbox.stub(feed, "clearCache").returns(Promise.resolve());
      sandbox.stub(feed, "loadCachedData").returns(Promise.resolve());
      await feed.onAction({type: at.DISCOVERY_STREAM_CONFIG_CHANGE});

      assert.calledOnce(feed.loadCachedData);
      assert.calledOnce(feed.clearCache);
      assert.isTrue(feed.loaded);
    });
    it("should clear the cache if a config change happens and config.enabled is true", async () => {
      // force clear cached pref value
      feed._prefCache = {};
      configPrefStub.returns(JSON.stringify({enabled: true}));

      sandbox.stub(feed, "clearCache").returns(Promise.resolve());
      await feed.onAction({type: at.DISCOVERY_STREAM_CONFIG_CHANGE});

      assert.calledOnce(feed.clearCache);
    });
    it("should not call this.loadCachedData if config.enabled changes to false", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      // force clear cached pref value
      feed._prefCache = {};
      configPrefStub.returns(JSON.stringify({enabled: true}));

      await feed.onAction({type: at.INIT});
      assert.isTrue(feed.loaded);

      feed._prefCache = {};
      configPrefStub.returns(JSON.stringify({enabled: false}));
      sandbox.stub(feed, "clearCache").returns(Promise.resolve());
      sandbox.stub(feed, "loadCachedData").returns(Promise.resolve());
      await feed.onAction({type: at.DISCOVERY_STREAM_CONFIG_CHANGE});

      assert.notCalled(feed.loadCachedData);
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
