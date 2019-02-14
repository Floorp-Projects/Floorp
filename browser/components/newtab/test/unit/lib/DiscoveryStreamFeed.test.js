import {actionCreators as ac, actionTypes as at, actionUtils as au} from "common/Actions.jsm";
import {combineReducers, createStore} from "redux";
import {DiscoveryStreamFeed} from "lib/DiscoveryStreamFeed.jsm";
import {reducers} from "common/Reducers.jsm";

const CONFIG_PREF_NAME = "browser.newtabpage.activity-stream.discoverystream.config";
const SPOC_IMPRESSION_TRACKING_PREF = "browser.newtabpage.activity-stream.discoverystream.spoc.impressions";
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
      .returns(JSON.stringify({enabled: false, show_spocs: false, layout_endpoint: "foo.com"}));

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
      configPrefStub.returns(JSON.stringify({enabled: true, show_spocs: false, layout_endpoint: "foo"}));

      feed.observe(null, null, CONFIG_PREF_NAME);

      assert.deepEqual(feed.store.getState().DiscoveryStream.config, {enabled: true, show_spocs: false, layout_endpoint: "foo"});
    });
  });

  describe("#loadLayout", () => {
    it("should fetch data and populate the cache if it is empty", async () => {
      const resp = {layout: ["foo", "bar"]};
      const fakeCache = {};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      fetchStub.resolves({ok: true, json: () => Promise.resolve(resp)});

      await feed.loadLayout(feed.store.dispatch);

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
      await feed.loadLayout(feed.store.dispatch);

      assert.calledOnce(fetchStub);
      assert.calledWith(feed.cache.set, "layout", resp);
    });
    it("should use the cached data and not fetch if the cached data is less than 30 mins old", async () => {
      const fakeCache = {layout: {layout: ["hello"], _timestamp: Date.now()}};

      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      clock.tick(THIRTY_MINUTES - 1);
      await feed.loadLayout(feed.store.dispatch);

      assert.notCalled(fetchStub);
      assert.notCalled(feed.cache.set);
    });
    it("should set spocs_endpoint from layout", async () => {
      const resp = {layout: ["foo", "bar"], spocs: {url: "foo.com"}};
      const fakeCache = {};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      fetchStub.resolves({ok: true, json: () => Promise.resolve(resp)});

      await feed.loadLayout(feed.store.dispatch);

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

      await feed.loadComponentFeeds(feed.store.dispatch);

      assert.calledWith(feed.cache.set, "feeds", {"foo.com": {"data": "data", "lastUpdated": 0}});
    });
  });

  describe("#getComponentFeed", () => {
    it("should fetch fresh data if cache is empty", async () => {
      const fakeCache = {};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed, "fetchFromEndpoint").resolves("data");

      const feedResp = await feed.getComponentFeed("foo.com");

      assert.equal(feedResp.data, "data");
    });
    it("should fetch fresh data if cache is old", async () => {
      const fakeCache = {feeds: {"foo.com": {lastUpdated: Date.now()}}};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed, "fetchFromEndpoint").resolves("data");
      clock.tick(THIRTY_MINUTES + 1);

      const feedResp = await feed.getComponentFeed("foo.com");

      assert.equal(feedResp.data, "data");
    });
    it("should return data from cache if it is fresh", async () => {
      const fakeCache = {feeds: {"foo.com": {lastUpdated: Date.now(), data: "data"}}};
      sandbox.stub(feed.cache, "get").resolves(fakeCache);
      sandbox.stub(feed, "fetchFromEndpoint").resolves("old data");
      clock.tick(THIRTY_MINUTES - 1);

      const feedResp = await feed.getComponentFeed("foo.com");

      assert.equal(feedResp.data, "data");
    });
    it("should return null if no response was received", async () => {
      sandbox.stub(feed, "fetchFromEndpoint").resolves(null);

      const feedResp = await feed.getComponentFeed("foo.com");

      assert.isNull(feedResp);
    });
  });

  describe("#loadSpocs", () => {
    beforeEach(() => {
      Object.defineProperty(feed, "showSpocs", {get: () => true});
    });
    it("should not fetch or update cache if no spocs endpoint is defined", async () => {
      feed.store.dispatch(ac.BroadcastToContent({
        type: at.DISCOVERY_STREAM_SPOCS_ENDPOINT,
        data: "",
      }));

      sandbox.spy(feed.cache, "set");

      await feed.loadSpocs(feed.store.dispatch);

      assert.notCalled(global.fetch);
      assert.notCalled(feed.cache.set);
    });
    it("should fetch fresh data if cache is empty", async () => {
      sandbox.stub(feed.cache, "get").returns(Promise.resolve());
      sandbox.stub(feed, "fetchFromEndpoint").resolves("data");
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      await feed.loadSpocs(feed.store.dispatch);

      assert.calledWith(feed.cache.set, "spocs", {"data": "data", "lastUpdated": 0});
      assert.equal(feed.store.getState().DiscoveryStream.spocs.data, "data");
    });
    it("should fetch fresh data if cache is old", async () => {
      const cachedSpoc = {"data": "old", "lastUpdated": Date.now()};
      const cachedData = {"spocs": cachedSpoc};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(cachedData));
      sandbox.stub(feed, "fetchFromEndpoint").resolves("new");
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      clock.tick(THIRTY_MINUTES + 1);

      await feed.loadSpocs(feed.store.dispatch);

      assert.equal(feed.store.getState().DiscoveryStream.spocs.data, "new");
    });
    it("should return data from cache if it is fresh", async () => {
      const cachedSpoc = {"data": "old", "lastUpdated": Date.now()};
      const cachedData = {"spocs": cachedSpoc};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(cachedData));
      sandbox.stub(feed, "fetchFromEndpoint").resolves("new");
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      clock.tick(THIRTY_MINUTES - 1);

      await feed.loadSpocs(feed.store.dispatch);

      assert.equal(feed.store.getState().DiscoveryStream.spocs.data, "old");
    });
  });

  describe("#showSpocs", () => {
    it("should return false from showSpocs if user pref showSponsored is false", async () => {
      feed.store.getState = () => ({
        Prefs: {values: {showSponsored: false}},
      });
      Object.defineProperty(feed, "config", {get: () => ({show_spocs: true})});

      assert.isFalse(feed.showSpocs);
    });
    it("should return false from showSpocs if DiscoveryStream pref show_spocs is false", async () => {
      feed.store.getState = () => ({
        Prefs: {values: {showSponsored: true}},
      });
      Object.defineProperty(feed, "config", {get: () => ({show_spocs: false})});

      assert.isFalse(feed.showSpocs);
    });
    it("should return true from showSpocs if both prefs are true", async () => {
      feed.store.getState = () => ({
        Prefs: {values: {showSponsored: true}},
      });
      Object.defineProperty(feed, "config", {get: () => ({show_spocs: true})});

      assert.isTrue(feed.showSpocs);
    });
    it("should fire loadSpocs is showSponsored pref changes", async () => {
      sandbox.stub(feed, "loadSpocs").returns(Promise.resolve());
      await feed.onAction({type: at.PREF_CHANGED, data: {name: "showSponsored"}});
      assert.calledOnce(feed.loadSpocs);
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

  describe("#filterSpocs", () => {
    it("should return filtered out spocs based on frequency caps", () => {
      const fakeSpocs = {
        spocs: [
          {
            campaign_id: "seen",
            caps: {
              lifetime: 3,
              campaign: {
                count: 1,
                period: 1,
              },
            },
          },
          {
            campaign_id: "not-seen",
            caps: {
              lifetime: 3,
              campaign: {
                count: 1,
                period: 1,
              },
            },
          },
        ],
      };
      const fakeImpressions = {
        "seen": [Date.now() - 1],
      };
      sandbox.stub(feed, "readImpressionsPref").returns(fakeImpressions);

      const result = feed.filterSpocs(fakeSpocs);

      assert.equal(result.spocs.length, 1);
      assert.equal(result.spocs[0].campaign_id, "not-seen");
    });
  });

  describe("#isBelowFrequencyCap", () => {
    it("should return true if there are no campaign impressions", () => {
      const fakeImpressions = {
        "seen": [Date.now() - 1],
      };
      const fakeSpoc = {
        campaign_id: "not-seen",
        caps: {
          lifetime: 3,
          campaign: {
            count: 1,
            period: 1,
          },
        },
      };

      const result = feed.isBelowFrequencyCap(fakeImpressions, fakeSpoc);

      assert.isTrue(result);
    });
    it("should return true if there are no campaign caps", () => {
      const fakeImpressions = {
        "seen": [Date.now() - 1],
      };
      const fakeSpoc = {
        campaign_id: "seen",
        caps: {
          lifetime: 3,
        },
      };

      const result = feed.isBelowFrequencyCap(fakeImpressions, fakeSpoc);

      assert.isTrue(result);
    });

    it("should return false if lifetime cap is hit", () => {
      const fakeImpressions = {
        "seen": [Date.now() - 1],
      };
      const fakeSpoc = {
        campaign_id: "seen",
        caps: {
          lifetime: 1,
          campaign: {
            count: 3,
            period: 1,
          },
        },
      };

      const result = feed.isBelowFrequencyCap(fakeImpressions, fakeSpoc);

      assert.isFalse(result);
    });

    it("should return false if time based cap is hit", () => {
      const fakeImpressions = {
        "seen": [Date.now() - 1],
      };
      const fakeSpoc = {
        campaign_id: "seen",
        caps: {
          lifetime: 3,
          campaign: {
            count: 1,
            period: 1,
          },
        },
      };

      const result = feed.isBelowFrequencyCap(fakeImpressions, fakeSpoc);

      assert.isFalse(result);
    });
  });

  describe("#recordCampaignImpression", () => {
    it("should return false if time based cap is hit", () => {
      sandbox.stub(feed, "readImpressionsPref").returns({});
      sandbox.stub(feed, "writeImpressionsPref").returns();

      feed.recordCampaignImpression("seen");

      assert.calledWith(feed.writeImpressionsPref, SPOC_IMPRESSION_TRACKING_PREF, {"seen": [0]});
    });
  });

  describe("#cleanUpCampaignImpressionPref", () => {
    it("should remove campaign-3 because it is no longer being used", async () => {
      const fakeSpocs = {
        spocs: [
          {
            campaign_id: "campaign-1",
            caps: {
              lifetime: 3,
              campaign: {
                count: 1,
                period: 1,
              },
            },
          },
          {
            campaign_id: "campaign-2",
            caps: {
              lifetime: 3,
              campaign: {
                count: 1,
                period: 1,
              },
            },
          },
        ],
      };
      const fakeImpressions = {
        "campaign-2": [Date.now() - 1],
        "campaign-3": [Date.now() - 1],
      };
      sandbox.stub(feed, "readImpressionsPref").returns(fakeImpressions);
      sandbox.stub(feed, "writeImpressionsPref").returns();

      feed.cleanUpCampaignImpressionPref(fakeSpocs);

      assert.calledWith(feed.writeImpressionsPref, SPOC_IMPRESSION_TRACKING_PREF, {"campaign-2": [-1]});
    });
  });

  describe("#writeImpressionsPref", () => {
    it("should call Services.prefs.setStringPref", () => {
      sandbox.stub(global.Services.prefs, "setStringPref").returns();
      const fakeImpressions = {
        "foo": [Date.now() - 1],
        "bar": [Date.now() - 1],
      };

      feed.writeImpressionsPref(SPOC_IMPRESSION_TRACKING_PREF, fakeImpressions);

      assert.calledWith(global.Services.prefs.setStringPref, SPOC_IMPRESSION_TRACKING_PREF, JSON.stringify(fakeImpressions));
    });
  });

  describe("#readImpressionsPref", () => {
    it("should return what's in Services.prefs.getStringPref", () => {
      const fakeImpressions = {
        "foo": [Date.now() - 1],
        "bar": [Date.now() - 1],
      };
      configPrefStub.withArgs(SPOC_IMPRESSION_TRACKING_PREF).returns(JSON.stringify(fakeImpressions));

      const result = feed.readImpressionsPref(SPOC_IMPRESSION_TRACKING_PREF);

      assert.deepEqual(result, fakeImpressions);
    });
  });

  describe("#onAction: DISCOVERY_STREAM_SPOC_IMPRESSION", () => {
    it("should call dispatch to ac.AlsoToPreloaded with filtered spoc data", async () => {
      Object.defineProperty(feed, "showSpocs", {get: () => true});
      const fakeSpocs = {
        lastUpdated: 1,
        data: {
          spocs: [
            {
              campaign_id: "seen",
              caps: {
                lifetime: 3,
                campaign: {
                  count: 1,
                  period: 1,
                },
              },
            },
            {
              campaign_id: "not-seen",
              caps: {
                lifetime: 3,
                campaign: {
                  count: 1,
                  period: 1,
                },
              },
            },
          ],
        },
      };
      const fakeImpressions = {
        "seen": [Date.now() - 1],
      };
      const result = {
        spocs: [
          {
            campaign_id: "not-seen",
            caps: {
              lifetime: 3,
              campaign: {
                count: 1,
                period: 1,
              },
            },
          },
        ],
      };
      sandbox.stub(feed, "recordCampaignImpression").returns();
      sandbox.stub(feed, "readImpressionsPref").returns(fakeImpressions);
      sandbox.stub(feed.cache, "get").returns(Promise.resolve({spocs: fakeSpocs}));
      sandbox.spy(feed.store, "dispatch");

      await feed.onAction({type: at.DISCOVERY_STREAM_SPOC_IMPRESSION, data: {campaign_id: "seen"}});

      assert.deepEqual(feed.store.dispatch.firstCall.args[0].data.spocs, result);
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

  describe("#onAction: SYSTEM_TICK", () => {
    it("should not refresh if DiscoveryStream has not been loaded", async () => {
      sandbox.stub(feed, "refreshAll").resolves();
      configPrefStub.returns(JSON.stringify({enabled: true}));

      await feed.onAction({type: at.SYSTEM_TICK});
      assert.notCalled(feed.refreshAll);
    });

    it("should not refresh if no caches are expired", async () => {
      sandbox.stub(feed.cache, "set").resolves();
      configPrefStub.returns(JSON.stringify({enabled: true}));

      await feed.onAction({type: at.INIT});

      sandbox.stub(feed, "checkIfAnyCacheExpired").resolves(false);
      sandbox.stub(feed, "refreshAll").resolves();

      await feed.onAction({type: at.SYSTEM_TICK});
      assert.notCalled(feed.refreshAll);
    });

    it("should refresh if DiscoveryStream has been loaded at least once and a cache has expired", async () => {
      sandbox.stub(feed.cache, "set").resolves();
      configPrefStub.returns(JSON.stringify({enabled: true}));

      await feed.onAction({type: at.INIT});

      sandbox.stub(feed, "checkIfAnyCacheExpired").resolves(true);
      sandbox.stub(feed, "refreshAll").resolves();

      await feed.onAction({type: at.SYSTEM_TICK});
      assert.calledOnce(feed.refreshAll);
    });

    it("should refresh and not update open tabs if DiscoveryStream has been loaded at least once", async () => {
      sandbox.stub(feed.cache, "set").resolves();
      configPrefStub.returns(JSON.stringify({enabled: true}));

      await feed.onAction({type: at.INIT});

      sandbox.stub(feed, "checkIfAnyCacheExpired").resolves(true);
      sandbox.stub(feed, "refreshAll").resolves();

      await feed.onAction({type: at.SYSTEM_TICK});
      assert.calledWith(feed.refreshAll, {updateOpenTabs: false});
    });
  });

  describe("#isExpired", () => {
    it("should throw if the key is not valid", () => {
      assert.throws(() => {
        feed.isExpired({}, "foo");
      });
    });
  });

  describe("#checkIfAnyCacheExpired", () => {
    let cache;
    beforeEach(() => {
      cache = {
        layout: {_timestamp: Date.now()},
        feeds: {"foo.com": {lastUpdated: Date.now()}},
        spocs: {lastUpdated: Date.now()},
      };
      sandbox.stub(feed.cache, "get").resolves(cache);
    });

    it("should return false if nothing in the cache is expired", async () => {
      const result = await feed.checkIfAnyCacheExpired();
      assert.isFalse(result);
    });

    it("should return true if .layout is missing", async () => {
      delete cache.layout;
      assert.isTrue(await feed.checkIfAnyCacheExpired());
    });
    it("should return true if .layout is expired", async () => {
      clock.tick(THIRTY_MINUTES + 1);
      // Update other caches we aren't testing
      cache.feeds["foo.com"].lastUpdate = Date.now();
      cache.spocs.lastUpdate = Date.now();

      assert.isTrue(await feed.checkIfAnyCacheExpired());
    });

    it("should return true if .spocs is missing", async () => {
      delete cache.spocs;
      assert.isTrue(await feed.checkIfAnyCacheExpired());
    });
    it("should return true if .spocs is expired", async () => {
      clock.tick(THIRTY_MINUTES + 1);
      // Update other caches we aren't testing
      cache.layout._timestamp = Date.now();
      cache.feeds["foo.com"].lastUpdate = Date.now();

      assert.isTrue(await feed.checkIfAnyCacheExpired());
    });

    it("should return true if .feeds is missing", async () => {
      delete cache.feeds;
      assert.isTrue(await feed.checkIfAnyCacheExpired());
    });
    it("should return true if data for .feeds[url] is missing", async () => {
      cache.feeds["foo.com"] = null;
      assert.isTrue(await feed.checkIfAnyCacheExpired());
    });
    it("should return true if data for .feeds[url] is expired", async () => {
      clock.tick(THIRTY_MINUTES + 1);
      // Update other caches we aren't testing
      cache.layout._timestamp = Date.now();
      cache.spocs.lastUpdate = Date.now();
      assert.isTrue(await feed.checkIfAnyCacheExpired());
    });
  });

  describe("#refreshAll", () => {
    beforeEach(() => {
      sandbox.stub(feed, "loadLayout").resolves();
      sandbox.stub(feed, "loadComponentFeeds").resolves();
      sandbox.stub(feed, "loadSpocs").resolves();
      sandbox.spy(feed.store, "dispatch");
    });

    it("should call layout, component, spocs update functions", async () => {
      await feed.refreshAll();

      assert.calledOnce(feed.loadLayout);
      assert.calledOnce(feed.loadComponentFeeds);
      assert.calledOnce(feed.loadSpocs);
    });
    it("should pass in dispatch wrapped with broadcast if options.updateOpenTabs is true", async () => {
      await feed.refreshAll({updateOpenTabs: true});
      [feed.loadLayout, feed.loadComponentFeeds, feed.loadSpocs]
        .forEach(fn => {
          assert.calledOnce(fn);
          const result = fn.firstCall.args[0]({type: "FOO"});
          assert.isTrue(au.isBroadcastToContent(result));
        });
    });
    it("should pass in dispatch with regular actions if options.updateOpenTabs is false", async () => {
      await feed.refreshAll({updateOpenTabs: false});
      [feed.loadLayout, feed.loadComponentFeeds, feed.loadSpocs]
        .forEach(fn => {
          assert.calledOnce(fn);
          const result = fn.firstCall.args[0]({type: "FOO"});
          assert.deepEqual(result, {type: "FOO"});
        });
    });
  });
});
