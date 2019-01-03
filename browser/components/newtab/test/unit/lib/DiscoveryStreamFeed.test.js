import {combineReducers, createStore} from "redux";
import {actionTypes as at} from "common/Actions.jsm";
import {DiscoveryStreamFeed} from "lib/DiscoveryStreamFeed.jsm";
import {reducers} from "common/Reducers.jsm";

const CONFIG_PREF_NAME = "browser.newtabpage.activity-stream.discoverystream.config";

describe("DiscoveryStreamFeed", () => {
  let feed;
  let sandbox;
  let configPrefStub;
  let fetchStub;

  beforeEach(() => {
    // Pref
    sandbox = sinon.createSandbox();
    configPrefStub = sandbox.stub(global.Services.prefs, "getStringPref")
      .withArgs(CONFIG_PREF_NAME)
      .returns(JSON.stringify({enabled: false, layout_endpoint: "foo.com"}));

    // Fetch
    fetchStub = sandbox.stub(global, "fetch");

    // Feed
    feed = new DiscoveryStreamFeed();
    feed.store = createStore(combineReducers(reducers));
  });

  afterEach(() => {
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
  describe("#onAction: DISCOVERY_STREAM_CONFIG_CHANGE", () => {
    it("should call this.loadCachedData if config.enabled changes to true ", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      // First initialize
      await feed.onAction({type: at.INIT});
      assert.isFalse(feed.loaded);

      // force clear cached pref value
      feed._prefCache = {};
      configPrefStub.returns(JSON.stringify({enabled: true}));

      sandbox.stub(feed, "loadCachedData").returns(Promise.resolve());
      await feed.onAction({type: at.DISCOVERY_STREAM_CONFIG_CHANGE});

      assert.calledOnce(feed.loadCachedData);
      assert.isTrue(feed.loaded);
    });
    it("should call this.loadCachedData if config.enabled changes to true ", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      // force clear cached pref value
      feed._prefCache = {};
      configPrefStub.returns(JSON.stringify({enabled: true}));

      await feed.onAction({type: at.INIT});
      assert.isTrue(feed.loaded);

      feed._prefCache = {};
      configPrefStub.returns(JSON.stringify({enabled: false}));
      sandbox.stub(feed, "loadCachedData").returns(Promise.resolve());
      await feed.onAction({type: at.DISCOVERY_STREAM_CONFIG_CHANGE});

      assert.notCalled(feed.loadCachedData);
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
