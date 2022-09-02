import {
  actionCreators as ac,
  actionTypes as at,
  actionUtils as au,
} from "common/Actions.jsm";
import { combineReducers, createStore } from "redux";
import { GlobalOverrider } from "test/unit/utils";
import { DiscoveryStreamFeed } from "lib/DiscoveryStreamFeed.jsm";
import { RecommendationProvider } from "lib/RecommendationProvider.jsm";
import { reducers } from "common/Reducers.jsm";

import { PersistentCache } from "lib/PersistentCache.jsm";
import { PersonalityProvider } from "lib/PersonalityProvider/PersonalityProvider.jsm";

const CONFIG_PREF_NAME = "discoverystream.config";
const DUMMY_ENDPOINT = "https://getpocket.cdn.mozilla.net/dummy";
const ENDPOINTS_PREF_NAME = "discoverystream.endpoints";
const SPOC_IMPRESSION_TRACKING_PREF = "discoverystream.spoc.impressions";
const REC_IMPRESSION_TRACKING_PREF = "discoverystream.rec.impressions";
const THIRTY_MINUTES = 30 * 60 * 1000;
const ONE_WEEK = 7 * 24 * 60 * 60 * 1000; // 1 week

const FAKE_UUID = "{foo-123-foo}";

// eslint-disable-next-line max-statements
describe("DiscoveryStreamFeed", () => {
  let feed;
  let feeds;
  let recommendationProvider;
  let sandbox;
  let fetchStub;
  let clock;
  let fakeNewTabUtils;
  let fakePktApi;
  let globals;

  const setPref = (name, value) => {
    const action = {
      type: at.PREF_CHANGED,
      data: {
        name,
        value: typeof value === "object" ? JSON.stringify(value) : value,
      },
    };
    feed.store.dispatch(action);
    feed.onAction(action);
  };

  beforeEach(() => {
    sandbox = sinon.createSandbox();

    // Fetch
    fetchStub = sandbox.stub(global, "fetch");

    // Time
    clock = sinon.useFakeTimers();

    globals = new GlobalOverrider();
    globals.set({
      gUUIDGenerator: { generateUUID: () => FAKE_UUID },
      PersistentCache,
      PersonalityProvider,
    });

    sandbox
      .stub(global.Services.prefs, "getBoolPref")
      .withArgs("browser.newtabpage.activity-stream.discoverystream.enabled")
      .returns(true);

    recommendationProvider = new RecommendationProvider();
    recommendationProvider.store = createStore(combineReducers(reducers), {});
    feeds = {
      "feeds.recommendationprovider": recommendationProvider,
    };

    // Feed
    feed = new DiscoveryStreamFeed();
    feed.store = createStore(combineReducers(reducers), {
      Prefs: {
        values: {
          [CONFIG_PREF_NAME]: JSON.stringify({
            enabled: false,
            show_spocs: false,
            layout_endpoint: DUMMY_ENDPOINT,
          }),
          [ENDPOINTS_PREF_NAME]: DUMMY_ENDPOINT,
          "discoverystream.enabled": true,
          "feeds.section.topstories": true,
          "feeds.system.topstories": true,
          "discoverystream.spocs.personalized": true,
          "discoverystream.recs.personalized": true,
        },
      },
    });
    feed.store.feeds = {
      get: name => feeds[name],
    };
    global.fetch.resetHistory();

    sandbox.stub(feed, "_maybeUpdateCachedData").resolves();

    globals.set("setTimeout", callback => {
      callback();
    });

    fakeNewTabUtils = {
      blockedLinks: {
        links: [],
        isBlocked: () => false,
      },
    };
    globals.set("NewTabUtils", fakeNewTabUtils);

    fakePktApi = {
      isUserLoggedIn: () => false,
      getRecentSavesCache: () => null,
      getRecentSaves: () => null,
    };
    globals.set("pktApi", fakePktApi);
  });

  afterEach(() => {
    clock.restore();
    sandbox.restore();
    globals.restore();
  });

  describe("#fetchFromEndpoint", () => {
    beforeEach(() => {
      feed._prefCache = {
        config: {
          api_key_pref: "",
        },
      };
      fetchStub.resolves({
        json: () => Promise.resolve("hi"),
        ok: true,
      });
    });
    it("should get a response", async () => {
      const response = await feed.fetchFromEndpoint(DUMMY_ENDPOINT);

      assert.equal(response, "hi");
    });
    it("should not send cookies", async () => {
      await feed.fetchFromEndpoint(DUMMY_ENDPOINT);

      assert.propertyVal(fetchStub.firstCall.args[1], "credentials", "omit");
    });
    it("should allow unexpected response", async () => {
      fetchStub.resolves({ ok: false });

      const response = await feed.fetchFromEndpoint(DUMMY_ENDPOINT);

      assert.equal(response, null);
    });
    it("should disallow unexpected endpoints", async () => {
      feed.store.getState = () => ({
        Prefs: { values: { [ENDPOINTS_PREF_NAME]: "https://other.site" } },
      });

      const response = await feed.fetchFromEndpoint(DUMMY_ENDPOINT);

      assert.equal(response, null);
    });
    it("should allow multiple endpoints", async () => {
      feed.store.getState = () => ({
        Prefs: {
          values: {
            [ENDPOINTS_PREF_NAME]: `https://other.site,${DUMMY_ENDPOINT}`,
          },
        },
      });

      const response = await feed.fetchFromEndpoint(DUMMY_ENDPOINT);

      assert.equal(response, "hi");
    });
    it("should replace urls with $apiKey", async () => {
      sandbox.stub(global.Services.prefs, "getCharPref").returns("replaced");

      await feed.fetchFromEndpoint(
        "https://getpocket.cdn.mozilla.net/dummy?consumer_key=$apiKey"
      );

      assert.calledWithMatch(
        fetchStub,
        "https://getpocket.cdn.mozilla.net/dummy?consumer_key=replaced",
        { credentials: "omit" }
      );
    });
    it("should replace locales with $locale", async () => {
      feed.locale = "replaced";
      await feed.fetchFromEndpoint(
        "https://getpocket.cdn.mozilla.net/dummy?locale_lang=$locale"
      );

      assert.calledWithMatch(
        fetchStub,
        "https://getpocket.cdn.mozilla.net/dummy?locale_lang=replaced",
        { credentials: "omit" }
      );
    });
    it("should allow POST and with other options", async () => {
      await feed.fetchFromEndpoint("https://getpocket.cdn.mozilla.net/dummy", {
        method: "POST",
        body: "{}",
      });

      assert.calledWithMatch(
        fetchStub,
        "https://getpocket.cdn.mozilla.net/dummy",
        {
          credentials: "omit",
          method: "POST",
          body: "{}",
        }
      );
    });
  });

  describe("#setupPocketState", () => {
    it("should setup logged in state and recent saves with cache", async () => {
      fakePktApi.isUserLoggedIn = () => true;
      fakePktApi.getRecentSavesCache = () => [1, 2, 3];
      sandbox.spy(feed.store, "dispatch");
      await feed.setupPocketState({});
      assert.calledTwice(feed.store.dispatch);
      assert.calledWith(
        feed.store.dispatch.firstCall,
        ac.OnlyToOneContent(
          {
            type: at.DISCOVERY_STREAM_POCKET_STATE_SET,
            data: { isUserLoggedIn: true },
          },
          {}
        )
      );
      assert.calledWith(
        feed.store.dispatch.secondCall,
        ac.OnlyToOneContent(
          {
            type: at.DISCOVERY_STREAM_RECENT_SAVES,
            data: { recentSaves: [1, 2, 3] },
          },
          {}
        )
      );
    });
    it("should setup logged in state and recent saves without cache", async () => {
      fakePktApi.isUserLoggedIn = () => true;
      fakePktApi.getRecentSaves = ({ success }) => success([1, 2, 3]);
      sandbox.spy(feed.store, "dispatch");
      await feed.setupPocketState({});
      assert.calledTwice(feed.store.dispatch);
      assert.calledWith(
        feed.store.dispatch.firstCall,
        ac.OnlyToOneContent(
          {
            type: at.DISCOVERY_STREAM_POCKET_STATE_SET,
            data: { isUserLoggedIn: true },
          },
          {}
        )
      );
      assert.calledWith(
        feed.store.dispatch.secondCall,
        ac.OnlyToOneContent(
          {
            type: at.DISCOVERY_STREAM_RECENT_SAVES,
            data: { recentSaves: [1, 2, 3] },
          },
          {}
        )
      );
    });
  });

  describe("#getOrCreateImpressionId", () => {
    it("should create impression id in constructor", async () => {
      assert.equal(feed._impressionId, FAKE_UUID);
    });
    it("should create impression id if none exists", async () => {
      sandbox.stub(global.Services.prefs, "setCharPref").returns();

      const result = feed.getOrCreateImpressionId();

      assert.equal(result, FAKE_UUID);
      assert.calledOnce(global.Services.prefs.setCharPref);
    });
    it("should use impression id if exists", async () => {
      sandbox.stub(global.Services.prefs, "getCharPref").returns("from get");

      const result = feed.getOrCreateImpressionId();

      assert.equal(result, "from get");
      assert.calledOnce(global.Services.prefs.getCharPref);
    });
  });

  describe("#parseGridPositions", () => {
    it("should return an equivalent array for an array of non negative integers", async () => {
      assert.deepEqual(feed.parseGridPositions([0, 2, 3]), [0, 2, 3]);
    });
    it("should return undefined for an array containing negative integers", async () => {
      assert.equal(feed.parseGridPositions([-2, 2, 3]), undefined);
    });
    it("should return undefined for an undefined input", async () => {
      assert.equal(feed.parseGridPositions(undefined), undefined);
    });
  });

  describe("#loadLayout", () => {
    it("should fetch data and populate the cache if it is empty", async () => {
      const resp = { layout: ["foo", "bar"] };
      const fakeCache = {};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      fetchStub.resolves({ ok: true, json: () => Promise.resolve(resp) });

      await feed.loadLayout(feed.store.dispatch);

      assert.calledOnce(fetchStub);
      assert.equal(feed.cache.set.firstCall.args[0], "layout");
      assert.deepEqual(feed.cache.set.firstCall.args[1].layout, resp.layout);
    });
    it("should fetch data and populate the cache if the cached data is older than 30 mins", async () => {
      const resp = { layout: ["foo", "bar"] };
      const fakeCache = {
        layout: { layout: ["hello"], lastUpdated: Date.now() },
      };

      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      fetchStub.resolves({ ok: true, json: () => Promise.resolve(resp) });

      clock.tick(THIRTY_MINUTES + 1);
      await feed.loadLayout(feed.store.dispatch);

      assert.calledOnce(fetchStub);
      assert.equal(feed.cache.set.firstCall.args[0], "layout");
      assert.deepEqual(feed.cache.set.firstCall.args[1].layout, resp.layout);
    });
    it("should use the cached data and not fetch if the cached data is less than 30 mins old", async () => {
      const fakeCache = {
        layout: { layout: ["hello"], lastUpdated: Date.now() },
      };

      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      clock.tick(THIRTY_MINUTES - 1);
      await feed.loadLayout(feed.store.dispatch);

      assert.notCalled(fetchStub);
      assert.notCalled(feed.cache.set);
    });
    it("should set spocs_endpoint from layout", async () => {
      const resp = { layout: ["foo", "bar"], spocs: { url: "foo.com" } };
      const fakeCache = {};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      fetchStub.resolves({ ok: true, json: () => Promise.resolve(resp) });

      await feed.loadLayout(feed.store.dispatch);

      assert.equal(
        feed.store.getState().DiscoveryStream.spocs.spocs_endpoint,
        "foo.com"
      );
    });
    it("should use local layout with hardcoded_layout being true", async () => {
      feed.config.hardcoded_layout = true;
      sandbox.stub(feed, "fetchLayout").returns(Promise.resolve(""));

      await feed.loadLayout(feed.store.dispatch);

      assert.notCalled(feed.fetchLayout);
      assert.equal(
        feed.store.getState().DiscoveryStream.spocs.spocs_endpoint,
        "https://spocs.getpocket.com/spocs"
      );
    });
    it("should use local basic layout with hardcoded_layout and hardcoded_basic_layout being true", async () => {
      feed.config.hardcoded_layout = true;
      feed.config.hardcoded_basic_layout = true;
      sandbox.stub(feed, "fetchLayout").returns(Promise.resolve(""));

      await feed.loadLayout(feed.store.dispatch);

      assert.notCalled(feed.fetchLayout);
      assert.equal(
        feed.store.getState().DiscoveryStream.spocs.spocs_endpoint,
        "https://spocs.getpocket.com/spocs"
      );
      const { layout } = feed.store.getState().DiscoveryStream;
      assert.equal(layout[0].components[2].properties.items, 3);
    });
    it("should use 1 row layout if specified", async () => {
      feed.config.hardcoded_layout = true;
      feed.store = createStore(combineReducers(reducers), {
        Prefs: {
          values: {
            [CONFIG_PREF_NAME]: JSON.stringify({
              enabled: true,
              show_spocs: false,
              layout_endpoint: DUMMY_ENDPOINT,
            }),
            [ENDPOINTS_PREF_NAME]: DUMMY_ENDPOINT,
            "discoverystream.enabled": true,
            "discoverystream.region-basic-layout": true,
          },
        },
      });
      sandbox.stub(feed, "fetchLayout").returns(Promise.resolve(""));

      await feed.loadLayout(feed.store.dispatch);

      const { layout } = feed.store.getState().DiscoveryStream;
      assert.equal(layout[0].components[2].properties.items, 3);
    });
    it("should use 7 row layout if specified", async () => {
      feed.config.hardcoded_layout = true;
      feed.store = createStore(combineReducers(reducers), {
        Prefs: {
          values: {
            [CONFIG_PREF_NAME]: JSON.stringify({
              enabled: true,
              show_spocs: false,
              layout_endpoint: DUMMY_ENDPOINT,
            }),
            [ENDPOINTS_PREF_NAME]: DUMMY_ENDPOINT,
            "discoverystream.enabled": true,
            "discoverystream.region-basic-layout": false,
          },
        },
      });
      sandbox.stub(feed, "fetchLayout").returns(Promise.resolve(""));

      await feed.loadLayout(feed.store.dispatch);

      const { layout } = feed.store.getState().DiscoveryStream;
      assert.equal(layout[0].components[2].properties.items, 21);
    });
    it("should use new spocs endpoint if in the config", async () => {
      feed.config.spocs_endpoint = "https://spocs.getpocket.com/spocs2";

      await feed.loadLayout(feed.store.dispatch);

      assert.equal(
        feed.store.getState().DiscoveryStream.spocs.spocs_endpoint,
        "https://spocs.getpocket.com/spocs2"
      );
    });
    it("should use local basic layout with hardcoded_layout and FF pref hardcoded_basic_layout", async () => {
      feed.config.hardcoded_layout = true;
      feed.store = createStore(combineReducers(reducers), {
        Prefs: {
          values: {
            [CONFIG_PREF_NAME]: JSON.stringify({
              enabled: false,
              show_spocs: false,
              layout_endpoint: DUMMY_ENDPOINT,
            }),
            [ENDPOINTS_PREF_NAME]: DUMMY_ENDPOINT,
            "discoverystream.enabled": true,
            "discoverystream.hardcoded-basic-layout": true,
          },
        },
      });

      sandbox.stub(feed, "fetchLayout").returns(Promise.resolve(""));

      await feed.loadLayout(feed.store.dispatch);

      assert.notCalled(feed.fetchLayout);
      assert.equal(
        feed.store.getState().DiscoveryStream.spocs.spocs_endpoint,
        "https://spocs.getpocket.com/spocs"
      );
      const { layout } = feed.store.getState().DiscoveryStream;
      assert.equal(layout[0].components[2].properties.items, 3);
    });
    it("should use new spocs endpoint if in a FF pref", async () => {
      feed.store = createStore(combineReducers(reducers), {
        Prefs: {
          values: {
            [CONFIG_PREF_NAME]: JSON.stringify({
              enabled: false,
              show_spocs: false,
              layout_endpoint: DUMMY_ENDPOINT,
            }),
            [ENDPOINTS_PREF_NAME]: DUMMY_ENDPOINT,
            "discoverystream.enabled": true,
            "discoverystream.spocs-endpoint":
              "https://spocs.getpocket.com/spocs2",
          },
        },
      });

      await feed.loadLayout(feed.store.dispatch);

      assert.equal(
        feed.store.getState().DiscoveryStream.spocs.spocs_endpoint,
        "https://spocs.getpocket.com/spocs2"
      );
    });
    it("should fetch local layout for invalid layout endpoint or when fetch layout fails", async () => {
      feed.config.hardcoded_layout = false;
      fetchStub.resolves({ ok: false });

      await feed.loadLayout(feed.store.dispatch, true);

      assert.calledOnce(fetchStub);
      assert.equal(
        feed.store.getState().DiscoveryStream.spocs.spocs_endpoint,
        "https://spocs.getpocket.com/spocs"
      );
    });
    it("should return enough stories to fill a four card layout", async () => {
      feed.config.hardcoded_layout = true;

      feed.store = createStore(combineReducers(reducers), {
        Prefs: {
          values: {
            pocketConfig: { fourCardLayout: true },
          },
        },
      });

      sandbox.stub(feed, "fetchLayout").returns(Promise.resolve(""));

      await feed.loadLayout(feed.store.dispatch);

      const { layout } = feed.store.getState().DiscoveryStream;
      assert.equal(layout[0].components[2].properties.items, 24);
    });
    it("should create a layout with spoc and widget positions", async () => {
      feed.config.hardcoded_layout = true;
      feed.store = createStore(combineReducers(reducers), {
        Prefs: {
          values: {
            pocketConfig: {
              spocPositions: "1, 2",
              widgetPositions: "3, 4",
            },
          },
        },
      });

      await feed.loadLayout(feed.store.dispatch);

      const { layout } = feed.store.getState().DiscoveryStream;
      assert.deepEqual(layout[0].components[2].spocs.positions, [
        { index: 1 },
        { index: 2 },
      ]);
      assert.deepEqual(layout[0].components[2].widgets.positions, [
        { index: 3 },
        { index: 4 },
      ]);
    });
  });

  describe("#updatePlacements", () => {
    it("should fire update placements without dupes with updatePlacements", () => {
      sandbox.spy(feed.store, "dispatch");
      const fakeComponents = {
        components: [
          { placement: { name: "first" } },
          { placement: { name: "second" } },
        ],
      };
      const fakeLayout = [fakeComponents];

      feed.updatePlacements(feed.store.dispatch, fakeLayout);

      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, {
        type: "DISCOVERY_STREAM_SPOCS_PLACEMENTS",
        data: { placements: [{ name: "first" }, { name: "second" }] },
        meta: { isStartup: false },
      });
    });
    it("should fire update placements from loadLayout", async () => {
      sandbox.spy(feed, "updatePlacements");

      await feed.loadLayout(feed.store.dispatch);

      assert.calledOnce(feed.updatePlacements);
    });
  });

  describe("#placementsForEach", () => {
    it("should forEach through placements", () => {
      const fakeComponents = {
        components: [
          { placement: { name: "first" } },
          { placement: { name: "second" } },
        ],
      };
      const fakeLayout = [fakeComponents];
      feed.updatePlacements(feed.store.dispatch, fakeLayout);
      let items = [];

      feed.placementsForEach(item => items.push(item.name));

      assert.deepEqual(items, ["first", "second"]);
    });
    it("should forEach through placements for just spocs if no placements exist", () => {
      let items = [];

      feed.placementsForEach(item => items.push(item.name));

      assert.deepEqual(items, ["spocs"]);
    });
  });

  describe("#loadLayoutEndPointUsingPref", () => {
    it("should return endpoint if valid key", async () => {
      const endpoint = feed.finalLayoutEndpoint(
        "https://somedomain.org/stories?consumer_key=$apiKey",
        "test_key_val"
      );
      assert.equal(
        "https://somedomain.org/stories?consumer_key=test_key_val",
        endpoint
      );
    });

    it("should throw error if key is empty", async () => {
      assert.throws(() => {
        feed.finalLayoutEndpoint(
          "https://somedomain.org/stories?consumer_key=$apiKey",
          ""
        );
      });
    });

    it("should return url if $apiKey is missing in layout_endpoint", async () => {
      const endpoint = feed.finalLayoutEndpoint(
        "https://somedomain.org/stories?consumer_key=",
        "test_key_val"
      );
      assert.equal("https://somedomain.org/stories?consumer_key=", endpoint);
    });

    it("should update config layout_endpoint based on api_key_pref value", async () => {
      feed.store.getState = () => ({
        Prefs: {
          values: {
            [CONFIG_PREF_NAME]: JSON.stringify({
              api_key_pref: "test_api_key_pref",
              enabled: true,
              layout_endpoint:
                "https://somedomain.org/stories?consumer_key=$apiKey",
            }),
          },
        },
      });
      sandbox
        .stub(global.Services.prefs, "getCharPref")
        .returns("test_api_key_val");
      assert.equal(
        "https://somedomain.org/stories?consumer_key=test_api_key_val",
        feed.config.layout_endpoint
      );
    });

    it("should not update config layout_endpoint if api_key_pref missing", async () => {
      feed.store.getState = () => ({
        Prefs: {
          values: {
            [CONFIG_PREF_NAME]: JSON.stringify({
              enabled: true,
              layout_endpoint:
                "https://somedomain.org/stories?consumer_key=1234",
            }),
          },
        },
      });
      sandbox
        .stub(global.Services.prefs, "getCharPref")
        .returns("test_api_key_val");
      assert.notCalled(global.Services.prefs.getCharPref);
      assert.equal(
        "https://somedomain.org/stories?consumer_key=1234",
        feed.config.layout_endpoint
      );
    });

    it("should not set config layout_endpoint if layout_endpoint missing in prefs", async () => {
      feed.store.getState = () => ({
        Prefs: {
          values: {
            [CONFIG_PREF_NAME]: JSON.stringify({
              enabled: true,
            }),
          },
        },
      });
      sandbox
        .stub(global.Services.prefs, "getCharPref")
        .returns("test_api_key_val");
      assert.notCalled(global.Services.prefs.getCharPref);
      assert.isUndefined(feed.config.layout_endpoint);
    });
  });

  describe("#loadComponentFeeds", () => {
    let fakeCache;
    let fakeDiscoveryStream;
    beforeEach(() => {
      fakeDiscoveryStream = {
        DiscoveryStream: {
          layout: [
            { components: [{ feed: { url: "foo.com" } }] },
            { components: [{}] },
            {},
          ],
        },
      };
      fakeCache = {};
      sandbox.stub(feed.store, "getState").returns(fakeDiscoveryStream);
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
    });

    afterEach(() => {
      sandbox.restore();
    });

    it("should not dispatch updates when layout is not defined", async () => {
      fakeDiscoveryStream = {
        DiscoveryStream: {},
      };
      feed.store.getState.returns(fakeDiscoveryStream);
      sandbox.spy(feed.store, "dispatch");

      await feed.loadComponentFeeds(feed.store.dispatch);

      assert.notCalled(feed.store.dispatch);
    });

    it("should populate feeds cache", async () => {
      fakeCache = {
        feeds: { "foo.com": { lastUpdated: Date.now(), data: "data" } },
      };
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));

      await feed.loadComponentFeeds(feed.store.dispatch);

      assert.calledWith(feed.cache.set, "feeds", {
        "foo.com": { data: "data", lastUpdated: 0 },
      });
    });

    it("should send feed update events with new feed data", async () => {
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.spy(feed.store, "dispatch");
      feed._prefCache = {
        config: {
          api_key_pref: "",
        },
      };

      await feed.loadComponentFeeds(feed.store.dispatch);

      assert.calledWith(feed.store.dispatch.firstCall, {
        type: at.DISCOVERY_STREAM_FEED_UPDATE,
        data: { feed: { data: { status: "failed" } }, url: "foo.com" },
        meta: { isStartup: false },
      });
      assert.calledWith(feed.store.dispatch.secondCall, {
        type: at.DISCOVERY_STREAM_FEEDS_UPDATE,
        meta: { isStartup: false },
      });
    });

    it("should return number of promises equal to unique urls", async () => {
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(global.Promise, "all").resolves();
      fakeDiscoveryStream = {
        DiscoveryStream: {
          layout: [
            {
              components: [
                { feed: { url: "foo.com" } },
                { feed: { url: "bar.com" } },
              ],
            },
            { components: [{ feed: { url: "foo.com" } }] },
            {},
            { components: [{ feed: { url: "baz.com" } }] },
          ],
        },
      };
      feed.store.getState.returns(fakeDiscoveryStream);

      await feed.loadComponentFeeds(feed.store.dispatch);

      assert.calledOnce(global.Promise.all);
      const { args } = global.Promise.all.firstCall;
      assert.equal(args[0].length, 3);
    });
  });

  describe("#getComponentFeed", () => {
    it("should fetch fresh feed data if cache is empty", async () => {
      const fakeCache = {};
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed, "rotate").callsFake(val => val);
      sandbox
        .stub(feed, "scoreItems")
        .callsFake(val => ({ data: val, filtered: [] }));
      sandbox.stub(feed, "fetchFromEndpoint").resolves({
        recommendations: "data",
        settings: {
          recsExpireTime: 1,
        },
      });

      const feedResp = await feed.getComponentFeed("foo.com");

      assert.equal(feedResp.data.recommendations, "data");
    });
    it("should fetch fresh feed data if cache is old", async () => {
      const fakeCache = { feeds: { "foo.com": { lastUpdated: Date.now() } } };
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      sandbox.stub(feed, "fetchFromEndpoint").resolves({
        recommendations: "data",
        settings: {
          recsExpireTime: 1,
        },
      });
      sandbox.stub(feed, "rotate").callsFake(val => val);
      sandbox
        .stub(feed, "scoreItems")
        .callsFake(val => ({ data: val, filtered: [] }));
      clock.tick(THIRTY_MINUTES + 1);

      const feedResp = await feed.getComponentFeed("foo.com");

      assert.equal(feedResp.data.recommendations, "data");
    });
    it("should return feed data from cache if it is fresh", async () => {
      const fakeCache = {
        feeds: { "foo.com": { lastUpdated: Date.now(), data: "data" } },
      };
      sandbox.stub(feed.cache, "get").resolves(fakeCache);
      sandbox.stub(feed, "fetchFromEndpoint").resolves("old data");
      clock.tick(THIRTY_MINUTES - 1);

      const feedResp = await feed.getComponentFeed("foo.com");

      assert.equal(feedResp.data, "data");
    });
    it("should return null if no response was received", async () => {
      sandbox.stub(feed, "fetchFromEndpoint").resolves(null);

      const feedResp = await feed.getComponentFeed("foo.com");

      assert.deepEqual(feedResp, { data: { status: "failed" } });
    });
  });

  describe("#personalizationOverride", () => {
    it("should dispatch setPref", async () => {
      sandbox.spy(feed.store, "dispatch");
      feed.store.getState = () => ({
        Prefs: {
          values: {
            "discoverystream.personalization.enabled": true,
          },
        },
      });

      feed.personalizationOverride(true);

      assert.calledWithMatch(feed.store.dispatch, {
        data: {
          name: "discoverystream.personalization.override",
          value: true,
        },
        type: at.SET_PREF,
      });
    });
    it("should dispatch CLEAR_PREF", async () => {
      sandbox.spy(feed.store, "dispatch");
      feed.store.getState = () => ({
        Prefs: {
          values: {
            "discoverystream.personalization.enabled": true,
            "discoverystream.personalization.override": true,
          },
        },
      });

      feed.personalizationOverride(false);

      assert.calledWithMatch(feed.store.dispatch, {
        data: {
          name: "discoverystream.personalization.override",
        },
        type: at.CLEAR_PREF,
      });
    });
  });

  describe("#loadSpocs", () => {
    beforeEach(() => {
      feed._prefCache = {
        config: {
          api_key_pref: "",
        },
      };
      Object.defineProperty(feed, "showSpocs", { get: () => true });
    });
    it("should not fetch or update cache if no spocs endpoint is defined", async () => {
      feed.store.dispatch(
        ac.BroadcastToContent({
          type: at.DISCOVERY_STREAM_SPOCS_ENDPOINT,
          data: "",
        })
      );

      sandbox.spy(feed.cache, "set");

      await feed.loadSpocs(feed.store.dispatch);

      assert.notCalled(global.fetch);
      assert.notCalled(feed.cache.set);
    });
    it("should fetch fresh spocs data if cache is empty", async () => {
      sandbox.stub(feed.cache, "get").returns(Promise.resolve());
      sandbox.stub(feed, "fetchFromEndpoint").resolves({ placement: "data" });
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      await feed.loadSpocs(feed.store.dispatch);

      assert.calledWith(feed.cache.set, "spocs", {
        spocs: { placement: "data" },
        lastUpdated: 0,
      });
      assert.equal(
        feed.store.getState().DiscoveryStream.spocs.data.placement,
        "data"
      );
    });
    it("should fetch fresh data if cache is old", async () => {
      const cachedSpoc = {
        spocs: { placement: "old" },
        lastUpdated: Date.now(),
      };
      const cachedData = { spocs: cachedSpoc };
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(cachedData));
      sandbox.stub(feed, "fetchFromEndpoint").resolves({ placement: "new" });
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      clock.tick(THIRTY_MINUTES + 1);

      await feed.loadSpocs(feed.store.dispatch);

      assert.equal(
        feed.store.getState().DiscoveryStream.spocs.data.placement,
        "new"
      );
    });
    it("should return spoc data from cache if it is fresh", async () => {
      const cachedSpoc = {
        spocs: { placement: "old" },
        lastUpdated: Date.now(),
      };
      const cachedData = { spocs: cachedSpoc };
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(cachedData));
      sandbox.stub(feed, "fetchFromEndpoint").resolves({ placement: "new" });
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      clock.tick(THIRTY_MINUTES - 1);

      await feed.loadSpocs(feed.store.dispatch);

      assert.equal(
        feed.store.getState().DiscoveryStream.spocs.data.placement,
        "old"
      );
    });
    it("should properly transform spocs using placements", async () => {
      sandbox.stub(feed.cache, "get").returns(Promise.resolve());
      sandbox
        .stub(feed, "fetchFromEndpoint")
        .resolves({ spocs: { items: [{ id: "data" }] } });
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      await feed.loadSpocs(feed.store.dispatch);

      assert.calledWith(feed.cache.set, "spocs", {
        spocs: {
          spocs: {
            context: "",
            title: "",
            sponsor: "",
            sponsored_by_override: undefined,
            items: [{ id: "data", score: 1 }],
          },
        },
        lastUpdated: 0,
      });

      assert.deepEqual(
        feed.store.getState().DiscoveryStream.spocs.data.spocs.items[0],
        { id: "data", score: 1 }
      );
    });
    it("should normalizeSpocsItems for older spoc data", async () => {
      sandbox.stub(feed.cache, "get").returns(Promise.resolve());
      sandbox
        .stub(feed, "fetchFromEndpoint")
        .resolves({ spocs: [{ id: "data" }] });
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      await feed.loadSpocs(feed.store.dispatch);

      assert.deepEqual(
        feed.store.getState().DiscoveryStream.spocs.data.spocs.items[0],
        { id: "data", score: 1 }
      );
    });
    it("should call personalizationVersionOverride with feature_flags", async () => {
      sandbox.stub(feed.cache, "get").returns(Promise.resolve());
      sandbox.stub(feed, "personalizationOverride").returns();
      sandbox
        .stub(feed, "fetchFromEndpoint")
        .resolves({ settings: { feature_flags: {} }, spocs: [{ id: "data" }] });
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      await feed.loadSpocs(feed.store.dispatch);

      assert.calledOnce(feed.personalizationOverride);
    });
    it("should return expected data if normalizeSpocsItems returns no spoc data", async () => {
      sandbox.stub(feed.cache, "get").returns(Promise.resolve());
      sandbox
        .stub(feed, "fetchFromEndpoint")
        .resolves({ placement1: [{ id: "data" }], placement2: [] });
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      const fakeComponents = {
        components: [
          { placement: { name: "placement1" } },
          { placement: { name: "placement2" } },
        ],
      };
      feed.updatePlacements(feed.store.dispatch, [fakeComponents]);

      await feed.loadSpocs(feed.store.dispatch);

      assert.deepEqual(feed.store.getState().DiscoveryStream.spocs.data, {
        placement1: {
          title: "",
          context: "",
          sponsor: "",
          sponsored_by_override: undefined,
          items: [{ id: "data", score: 1 }],
        },
        placement2: {
          title: "",
          context: "",
          items: [],
        },
      });
    });
    it("should use title and context on spoc data", async () => {
      sandbox.stub(feed.cache, "get").returns(Promise.resolve());
      sandbox.stub(feed, "fetchFromEndpoint").resolves({
        placement1: {
          title: "title",
          context: "context",
          sponsor: "",
          sponsored_by_override: undefined,
          items: [{ id: "data" }],
        },
      });
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      const fakeComponents = {
        components: [{ placement: { name: "placement1" } }],
      };
      feed.updatePlacements(feed.store.dispatch, [fakeComponents]);

      await feed.loadSpocs(feed.store.dispatch);

      assert.deepEqual(feed.store.getState().DiscoveryStream.spocs.data, {
        placement1: {
          title: "title",
          context: "context",
          sponsor: "",
          sponsored_by_override: undefined,
          items: [{ id: "data", score: 1 }],
        },
      });
    });
  });

  describe("#normalizeSpocsItems", () => {
    it("should return correct data if new data passed in", async () => {
      const spocs = {
        title: "title",
        context: "context",
        sponsor: "sponsor",
        sponsored_by_override: "override",
        items: [{ id: "id" }],
      };
      const result = feed.normalizeSpocsItems(spocs);
      assert.deepEqual(result, spocs);
    });
    it("should return normalized data if new data passed in without title or context", async () => {
      const spocs = {
        items: [{ id: "id" }],
      };
      const result = feed.normalizeSpocsItems(spocs);
      assert.deepEqual(result, {
        title: "",
        context: "",
        sponsor: "",
        sponsored_by_override: undefined,
        items: [{ id: "id" }],
      });
    });
    it("should return normalized data if old data passed in", async () => {
      const spocs = [{ id: "id" }];
      const result = feed.normalizeSpocsItems(spocs);
      assert.deepEqual(result, {
        title: "",
        context: "",
        sponsor: "",
        sponsored_by_override: undefined,
        items: [{ id: "id" }],
      });
    });
  });

  describe("#showSpocs", () => {
    it("should return false from showSpocs if user pref showSponsored is false", async () => {
      feed.store.getState = () => ({
        Prefs: { values: { showSponsored: false } },
      });
      Object.defineProperty(feed, "config", {
        get: () => ({ show_spocs: true }),
      });

      assert.isFalse(feed.showSpocs);
    });
    it("should return false from showSpocs if DiscoveryStream pref show_spocs is false", async () => {
      feed.store.getState = () => ({
        Prefs: { values: { showSponsored: true } },
      });
      Object.defineProperty(feed, "config", {
        get: () => ({ show_spocs: false }),
      });

      assert.isFalse(feed.showSpocs);
    });
    it("should return true from showSpocs if both prefs are true", async () => {
      feed.store.getState = () => ({
        Prefs: { values: { showSponsored: true } },
      });
      Object.defineProperty(feed, "config", {
        get: () => ({ show_spocs: true }),
      });

      assert.isTrue(feed.showSpocs);
    });
  });

  describe("#clearSpocs", () => {
    it("should not fail with no endpoint", async () => {
      sandbox.stub(feed.store, "getState").returns({
        Prefs: {
          values: { "discoverystream.endpointSpocsClear": null },
        },
      });
      sandbox.stub(feed, "fetchFromEndpoint").resolves(null);

      await feed.clearSpocs();

      assert.notCalled(feed.fetchFromEndpoint);
    });
    it("should call DELETE with endpoint", async () => {
      sandbox.stub(feed.store, "getState").returns({
        Prefs: {
          values: {
            "discoverystream.endpointSpocsClear": "https://spocs/user",
          },
        },
      });
      sandbox.stub(feed, "fetchFromEndpoint").resolves(null);
      feed._impressionId = "1234";

      await feed.clearSpocs();

      assert.equal(
        feed.fetchFromEndpoint.firstCall.args[0],
        "https://spocs/user"
      );
      assert.equal(feed.fetchFromEndpoint.firstCall.args[1].method, "DELETE");
      assert.equal(
        feed.fetchFromEndpoint.firstCall.args[1].body,
        '{"pocket_id":"1234"}'
      );
    });
  });

  describe("#rotate", () => {
    it("should move seen first story to the back of the response", () => {
      const recsExpireTime = 5600;
      const feedResponse = {
        recommendations: [
          {
            id: "first",
          },
          {
            id: "second",
          },
          {
            id: "third",
          },
          {
            id: "fourth",
          },
        ],
        settings: {
          recsExpireTime,
        },
      };
      const fakeImpressions = {
        first: Date.now() - recsExpireTime * 1000,
        third: Date.now(),
      };
      sandbox.stub(feed, "readDataPref").returns(fakeImpressions);

      const result = feed.rotate(
        feedResponse.recommendations,
        feedResponse.settings.recsExpireTime
      );

      assert.equal(result[3].id, "first");
    });
  });

  describe("#reset", () => {
    it("should fire all reset based functions", async () => {
      sandbox.stub(global.Services.obs, "removeObserver").returns();

      sandbox.stub(feed, "resetDataPrefs").returns();
      sandbox.stub(feed, "resetCache").returns(Promise.resolve());
      sandbox.stub(feed, "resetState").returns();

      feed.loaded = true;

      await feed.reset();

      assert.calledOnce(feed.resetDataPrefs);
      assert.calledOnce(feed.resetCache);
      assert.calledOnce(feed.resetState);
      assert.calledOnce(global.Services.obs.removeObserver);
    });
  });

  describe("#resetCache", () => {
    it("should set .layout, .feeds .spocs and .personalization to {}", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      await feed.resetCache();

      assert.callCount(feed.cache.set, 4);
      const firstCall = feed.cache.set.getCall(0);
      const secondCall = feed.cache.set.getCall(1);
      const thirdCall = feed.cache.set.getCall(2);
      const fourthCall = feed.cache.set.getCall(3);
      assert.deepEqual(firstCall.args, ["layout", {}]);
      assert.deepEqual(secondCall.args, ["feeds", {}]);
      assert.deepEqual(thirdCall.args, ["spocs", {}]);
      assert.deepEqual(fourthCall.args, ["personalization", {}]);
    });
  });

  describe("#scoreItems", () => {
    it("should return initial data if spocs are empty", async () => {
      const { data: result } = await feed.scoreItems([]);

      assert.equal(result.length, 0);
    });

    it("should sort based on item_score", async () => {
      const { data: result } = await feed.scoreItems([
        { id: 2, flight_id: 2, item_score: 0.8 },
        { id: 4, flight_id: 4, item_score: 0.5 },
        { id: 3, flight_id: 3, item_score: 0.7 },
        { id: 1, flight_id: 1, item_score: 0.9 },
      ]);

      assert.deepEqual(result, [
        { id: 1, flight_id: 1, item_score: 0.9, score: 0.9 },
        { id: 2, flight_id: 2, item_score: 0.8, score: 0.8 },
        { id: 3, flight_id: 3, item_score: 0.7, score: 0.7 },
        { id: 4, flight_id: 4, item_score: 0.5, score: 0.5 },
      ]);
    });

    it("should sort based on priority", async () => {
      const { data: result } = await feed.scoreItems([
        { id: 6, flight_id: 6, priority: 2, item_score: 0.7 },
        { id: 2, flight_id: 3, priority: 1, item_score: 0.2 },
        { id: 4, flight_id: 4, item_score: 0.6 },
        { id: 5, flight_id: 5, priority: 2, item_score: 0.8 },
        { id: 3, flight_id: 3, item_score: 0.8 },
        { id: 1, flight_id: 1, priority: 1, item_score: 0.3 },
      ]);

      assert.deepEqual(result, [
        {
          id: 1,
          flight_id: 1,
          priority: 1,
          score: 0.3,
          item_score: 0.3,
        },
        {
          id: 2,
          flight_id: 3,
          priority: 1,
          score: 0.2,
          item_score: 0.2,
        },
        {
          id: 5,
          flight_id: 5,
          priority: 2,
          score: 0.8,
          item_score: 0.8,
        },
        {
          id: 6,
          flight_id: 6,
          priority: 2,
          score: 0.7,
          item_score: 0.7,
        },
        { id: 3, flight_id: 3, item_score: 0.8, score: 0.8 },
        { id: 4, flight_id: 4, item_score: 0.6, score: 0.6 },
      ]);
    });

    it("should add a score prop to spocs", async () => {
      const { data: result } = await feed.scoreItems([
        { flight_id: 1, item_score: 0.9 },
      ]);

      assert.equal(result[0].score, 0.9);
    });
  });

  describe("#filterBlocked", () => {
    it("should return initial data if spocs are empty", () => {
      const { data: result } = feed.filterBlocked([]);

      assert.equal(result.length, 0);
    });
    it("should return initial data if links are not blocked", () => {
      const { data: result } = feed.filterBlocked([
        { url: "https://foo.com" },
        { url: "test.com" },
      ]);
      assert.equal(result.length, 2);
    });
    it("should return initial recommendations data if links are not blocked", () => {
      const { data: result } = feed.filterBlocked([
        { url: "https://foo.com" },
        { url: "test.com" },
      ]);
      assert.equal(result.length, 2);
    });
    it("filterRecommendations based on blockedlist by passing feed data", () => {
      fakeNewTabUtils.blockedLinks.links = [{ url: "https://foo.com" }];
      fakeNewTabUtils.blockedLinks.isBlocked = site =>
        fakeNewTabUtils.blockedLinks.links[0].url === site.url;

      const result = feed.filterRecommendations({
        lastUpdated: 4,
        data: {
          recommendations: [{ url: "https://foo.com" }, { url: "test.com" }],
        },
      });

      assert.equal(result.lastUpdated, 4);
      assert.lengthOf(result.data.recommendations, 1);
      assert.equal(result.data.recommendations[0].url, "test.com");
      assert.notInclude(
        result.data.recommendations,
        fakeNewTabUtils.blockedLinks.links[0]
      );
    });
  });

  describe("#frequencyCapSpocs", () => {
    it("should return filtered out spocs based on frequency caps", () => {
      const fakeSpocs = [
        {
          id: 1,
          flight_id: "seen",
          caps: {
            lifetime: 3,
            flight: {
              count: 1,
              period: 1,
            },
          },
        },
        {
          id: 2,
          flight_id: "not-seen",
          caps: {
            lifetime: 3,
            flight: {
              count: 1,
              period: 1,
            },
          },
        },
      ];
      const fakeImpressions = {
        seen: [Date.now() - 1],
      };
      sandbox.stub(feed, "readDataPref").returns(fakeImpressions);

      const { data: result, filtered } = feed.frequencyCapSpocs(fakeSpocs);

      assert.equal(result.length, 1);
      assert.equal(result[0].flight_id, "not-seen");
      assert.deepEqual(filtered, [fakeSpocs[0]]);
    });
    it("should return simple structure and do nothing with no spocs", () => {
      const { data: result, filtered } = feed.frequencyCapSpocs([]);

      assert.equal(result.length, 0);
      assert.equal(filtered.length, 0);
    });
  });

  describe("#migrateFlightId", () => {
    it("should migrate campaign to flight if no flight exists", () => {
      const fakeSpocs = [
        {
          id: 1,
          campaign_id: "campaign",
          caps: {
            lifetime: 3,
            campaign: {
              count: 1,
              period: 1,
            },
          },
        },
      ];
      const { data: result } = feed.migrateFlightId(fakeSpocs);

      assert.deepEqual(result[0], {
        id: 1,
        flight_id: "campaign",
        campaign_id: "campaign",
        caps: {
          lifetime: 3,
          flight: {
            count: 1,
            period: 1,
          },
          campaign: {
            count: 1,
            period: 1,
          },
        },
      });
    });
    it("should not migrate campaign to flight if caps or id don't exist", () => {
      const fakeSpocs = [{ id: 1 }];
      const { data: result } = feed.migrateFlightId(fakeSpocs);

      assert.deepEqual(result[0], { id: 1 });
    });
    it("should return simple structure and do nothing with no spocs", () => {
      const { data: result } = feed.migrateFlightId([]);

      assert.equal(result.length, 0);
    });
  });

  describe("#isBelowFrequencyCap", () => {
    it("should return true if there are no flight impressions", () => {
      const fakeImpressions = {
        seen: [Date.now() - 1],
      };
      const fakeSpoc = {
        flight_id: "not-seen",
        caps: {
          lifetime: 3,
          flight: {
            count: 1,
            period: 1,
          },
        },
      };

      const result = feed.isBelowFrequencyCap(fakeImpressions, fakeSpoc);

      assert.isTrue(result);
    });
    it("should return true if there are no flight caps", () => {
      const fakeImpressions = {
        seen: [Date.now() - 1],
      };
      const fakeSpoc = {
        flight_id: "seen",
        caps: {
          lifetime: 3,
        },
      };

      const result = feed.isBelowFrequencyCap(fakeImpressions, fakeSpoc);

      assert.isTrue(result);
    });

    it("should return false if lifetime cap is hit", () => {
      const fakeImpressions = {
        seen: [Date.now() - 1],
      };
      const fakeSpoc = {
        flight_id: "seen",
        caps: {
          lifetime: 1,
          flight: {
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
        seen: [Date.now() - 1],
      };
      const fakeSpoc = {
        flight_id: "seen",
        caps: {
          lifetime: 3,
          flight: {
            count: 1,
            period: 1,
          },
        },
      };

      const result = feed.isBelowFrequencyCap(fakeImpressions, fakeSpoc);

      assert.isFalse(result);
    });
  });

  describe("#retryFeed", () => {
    it("should retry a feed fetch", async () => {
      sandbox.stub(feed, "getComponentFeed").returns(Promise.resolve({}));
      sandbox.stub(feed, "filterRecommendations").returns({});
      sandbox.spy(feed.store, "dispatch");

      await feed.retryFeed({ url: "https://feed.com" });

      assert.calledOnce(feed.getComponentFeed);
      assert.calledOnce(feed.filterRecommendations);
      assert.calledOnce(feed.store.dispatch);
      assert.equal(
        feed.store.dispatch.firstCall.args[0].type,
        "DISCOVERY_STREAM_FEED_UPDATE"
      );
      assert.deepEqual(feed.store.dispatch.firstCall.args[0].data, {
        feed: {},
        url: "https://feed.com",
      });
    });
  });

  describe("#recordFlightImpression", () => {
    it("should return false if time based cap is hit", () => {
      sandbox.stub(feed, "readDataPref").returns({});
      sandbox.stub(feed, "writeDataPref").returns();

      feed.recordFlightImpression("seen");

      assert.calledWith(feed.writeDataPref, SPOC_IMPRESSION_TRACKING_PREF, {
        seen: [0],
      });
    });
  });

  describe("#recordBlockFlightId", () => {
    it("should call writeDataPref with new flight id added", () => {
      sandbox.stub(feed, "readDataPref").returns({ "1234": 1 });
      sandbox.stub(feed, "writeDataPref").returns();

      feed.recordBlockFlightId("5678");

      assert.calledOnce(feed.readDataPref);
      assert.calledWith(feed.writeDataPref, "discoverystream.flight.blocks", {
        "1234": 1,
        "5678": 1,
      });
    });
  });

  describe("#cleanUpFlightImpressionPref", () => {
    it("should remove flight-3 because it is no longer being used", async () => {
      const fakeSpocs = {
        spocs: {
          items: [
            {
              flight_id: "flight-1",
              caps: {
                lifetime: 3,
                flight: {
                  count: 1,
                  period: 1,
                },
              },
            },
            {
              flight_id: "flight-2",
              caps: {
                lifetime: 3,
                flight: {
                  count: 1,
                  period: 1,
                },
              },
            },
          ],
        },
      };
      const fakeImpressions = {
        "flight-2": [Date.now() - 1],
        "flight-3": [Date.now() - 1],
      };
      sandbox.stub(feed, "readDataPref").returns(fakeImpressions);
      sandbox.stub(feed, "writeDataPref").returns();

      feed.cleanUpFlightImpressionPref(fakeSpocs);

      assert.calledWith(feed.writeDataPref, SPOC_IMPRESSION_TRACKING_PREF, {
        "flight-2": [-1],
      });
    });
  });

  describe("#recordTopRecImpressions", () => {
    it("should add a rec id to the rec impression pref", () => {
      sandbox.stub(feed, "readDataPref").returns({});
      sandbox.stub(feed, "writeDataPref");

      feed.recordTopRecImpressions("rec");

      assert.calledWith(feed.writeDataPref, REC_IMPRESSION_TRACKING_PREF, {
        rec: 0,
      });
    });
    it("should not add an impression if it already exists", () => {
      sandbox.stub(feed, "readDataPref").returns({ rec: 4 });
      sandbox.stub(feed, "writeDataPref");

      feed.recordTopRecImpressions("rec");

      assert.notCalled(feed.writeDataPref);
    });
  });

  describe("#cleanUpTopRecImpressionPref", () => {
    it("should remove recs no longer being used", () => {
      const newFeeds = {
        "https://foo.com": {
          data: {
            recommendations: [
              {
                id: "rec1",
              },
              {
                id: "rec2",
              },
            ],
          },
        },
        "https://bar.com": {
          data: {
            recommendations: [
              {
                id: "rec3",
              },
              {
                id: "rec4",
              },
            ],
          },
        },
      };
      const fakeImpressions = {
        rec2: Date.now() - 1,
        rec3: Date.now() - 1,
        rec5: Date.now() - 1,
      };
      sandbox.stub(feed, "readDataPref").returns(fakeImpressions);
      sandbox.stub(feed, "writeDataPref").returns();

      feed.cleanUpTopRecImpressionPref(newFeeds);

      assert.calledWith(feed.writeDataPref, REC_IMPRESSION_TRACKING_PREF, {
        rec2: -1,
        rec3: -1,
      });
    });
  });

  describe("#writeDataPref", () => {
    it("should call Services.prefs.setStringPref", () => {
      sandbox.spy(feed.store, "dispatch");
      const fakeImpressions = {
        foo: [Date.now() - 1],
        bar: [Date.now() - 1],
      };

      feed.writeDataPref(SPOC_IMPRESSION_TRACKING_PREF, fakeImpressions);

      assert.calledWithMatch(feed.store.dispatch, {
        data: {
          name: SPOC_IMPRESSION_TRACKING_PREF,
          value: JSON.stringify(fakeImpressions),
        },
        type: at.SET_PREF,
      });
    });
  });

  describe("#addEndpointQuery", () => {
    const url = "https://spocs.getpocket.com/spocs";

    it("should return same url with no query", () => {
      const result = feed.addEndpointQuery(url, "");
      assert.equal(result, url);
    });

    it("should add multiple query params to standard url", () => {
      const params = "?first=first&second=second";
      const result = feed.addEndpointQuery(url, params);
      assert.equal(result, url + params);
    });

    it("should add multiple query params to url with a query already", () => {
      const params = "first=first&second=second";
      const initialParams = "?zero=zero";
      const result = feed.addEndpointQuery(
        `${url}${initialParams}`,
        `?${params}`
      );
      assert.equal(result, `${url}${initialParams}&${params}`);
    });
  });

  describe("#readDataPref", () => {
    it("should return what's in Services.prefs.getStringPref", () => {
      const fakeImpressions = {
        foo: [Date.now() - 1],
        bar: [Date.now() - 1],
      };
      setPref(SPOC_IMPRESSION_TRACKING_PREF, fakeImpressions);

      const result = feed.readDataPref(SPOC_IMPRESSION_TRACKING_PREF);

      assert.deepEqual(result, fakeImpressions);
    });
  });

  describe("#setupPrefs", () => {
    it("should call setupPrefs", async () => {
      sandbox.spy(feed, "setupPrefs");
      feed.onAction({
        type: at.INIT,
      });
      assert.calledOnce(feed.setupPrefs);
    });
    it("should dispatch to at.DISCOVERY_STREAM_PREFS_SETUP with proper data", async () => {
      sandbox.spy(feed.store, "dispatch");
      globals.set("ExperimentAPI", {
        getExperiment: () => ({
          slug: "experimentId",
          branch: {
            slug: "branchId",
          },
        }),
      });
      global.Services.prefs.getBoolPref
        .withArgs("extensions.pocket.enabled")
        .returns(true);
      feed.store.getState = () => ({
        Prefs: {
          values: {
            region: "CA",
            pocketConfig: {
              recentSavesEnabled: true,
              hideDescriptions: false,
              hideDescriptionsRegions: "US,CA,GB",
              compactImages: true,
              imageGradient: true,
              newSponsoredLabel: true,
              titleLines: "1",
              descLines: "1",
              readTime: true,
              saveToPocketCard: false,
              saveToPocketCardRegions: "US,CA,GB",
            },
          },
        },
      });
      feed.setupPrefs();
      assert.deepEqual(feed.store.dispatch.firstCall.args[0].data, {
        utmSource: "pocket-newtab",
        utmCampaign: "experimentId",
        utmContent: "branchId",
      });
      assert.deepEqual(feed.store.dispatch.secondCall.args[0].data, {
        recentSavesEnabled: true,
        pocketButtonEnabled: true,
        saveToPocketCard: true,
        hideDescriptions: true,
        compactImages: true,
        imageGradient: true,
        newSponsoredLabel: true,
        titleLines: "1",
        descLines: "1",
        readTime: true,
      });
    });
  });

  describe("#onAction: DISCOVERY_STREAM_IMPRESSION_STATS", () => {
    it("should call recordTopRecImpressions from DISCOVERY_STREAM_IMPRESSION_STATS", async () => {
      sandbox.stub(feed, "recordTopRecImpressions").returns();
      await feed.onAction({
        type: at.DISCOVERY_STREAM_IMPRESSION_STATS,
        data: { tiles: [{ id: "seen" }] },
      });

      assert.calledWith(feed.recordTopRecImpressions, "seen");
    });
  });

  describe("#onAction: DISCOVERY_STREAM_SPOC_IMPRESSION", () => {
    beforeEach(() => {
      const data = {
        spocs: {
          items: [
            {
              id: 1,
              flight_id: "seen",
              caps: {
                lifetime: 3,
                flight: {
                  count: 1,
                  period: 1,
                },
              },
            },
            {
              id: 2,
              flight_id: "not-seen",
              caps: {
                lifetime: 3,
                flight: {
                  count: 1,
                  period: 1,
                },
              },
            },
          ],
        },
      };
      sandbox.stub(feed.store, "getState").returns({
        DiscoveryStream: {
          spocs: {
            data,
          },
        },
      });
    });

    it("should call dispatch to ac.AlsoToPreloaded with filtered spoc data", async () => {
      Object.defineProperty(feed, "showSpocs", { get: () => true });
      const fakeImpressions = {
        seen: [Date.now() - 1],
      };
      const result = {
        spocs: {
          items: [
            {
              id: 2,
              flight_id: "not-seen",
              caps: {
                lifetime: 3,
                flight: {
                  count: 1,
                  period: 1,
                },
              },
            },
          ],
        },
      };
      sandbox.stub(feed, "recordFlightImpression").returns();
      sandbox.stub(feed, "readDataPref").returns(fakeImpressions);
      sandbox.spy(feed.store, "dispatch");

      await feed.onAction({
        type: at.DISCOVERY_STREAM_SPOC_IMPRESSION,
        data: { flightId: "seen" },
      });

      assert.deepEqual(
        feed.store.dispatch.secondCall.args[0].data.spocs,
        result
      );
    });
    it("should not call dispatch to ac.AlsoToPreloaded if spocs were not changed by frequency capping", async () => {
      Object.defineProperty(feed, "showSpocs", { get: () => true });
      const fakeImpressions = {};
      sandbox.stub(feed, "recordFlightImpression").returns();
      sandbox.stub(feed, "readDataPref").returns(fakeImpressions);
      sandbox.spy(feed.store, "dispatch");

      await feed.onAction({
        type: at.DISCOVERY_STREAM_SPOC_IMPRESSION,
        data: { flight_id: "seen" },
      });

      assert.notCalled(feed.store.dispatch);
    });
    it("should attempt feq cap on valid spocs with placements on impression", async () => {
      sandbox.restore();
      Object.defineProperty(feed, "showSpocs", { get: () => true });
      const fakeImpressions = {};
      sandbox.stub(feed, "recordFlightImpression").returns();
      sandbox.stub(feed, "readDataPref").returns(fakeImpressions);
      sandbox.spy(feed.store, "dispatch");
      sandbox.spy(feed, "frequencyCapSpocs");

      const data = {
        spocs: {
          items: [
            {
              id: 2,
              flight_id: "seen-2",
              caps: {
                lifetime: 3,
                flight: {
                  count: 1,
                  period: 1,
                },
              },
            },
          ],
        },
      };
      sandbox.stub(feed.store, "getState").returns({
        DiscoveryStream: {
          spocs: {
            data,
            placements: [{ name: "spocs" }, { name: "notSpocs" }],
          },
        },
      });

      await feed.onAction({
        type: at.DISCOVERY_STREAM_SPOC_IMPRESSION,
        data: { flight_id: "doesn't matter" },
      });

      assert.calledOnce(feed.frequencyCapSpocs);
      assert.calledWith(feed.frequencyCapSpocs, data.spocs.items);
    });
  });

  describe("#onAction: PLACES_LINK_BLOCKED", () => {
    beforeEach(() => {
      const data = {
        spocs: {
          items: [
            {
              id: 1,
              flight_id: "foo",
              url: "foo.com",
            },
            {
              id: 2,
              flight_id: "bar",
              url: "bar.com",
            },
          ],
        },
      };
      sandbox.stub(feed.store, "getState").returns({
        DiscoveryStream: {
          spocs: {
            data,
            placements: [{ name: "spocs" }],
          },
        },
      });
    });

    it("should call dispatch if found a blocked spoc", async () => {
      Object.defineProperty(feed, "showSpocs", { get: () => true });

      sandbox.spy(feed.store, "dispatch");

      await feed.onAction({
        type: at.PLACES_LINK_BLOCKED,
        data: { url: "foo.com" },
      });

      assert.deepEqual(
        feed.store.dispatch.firstCall.args[0].data.url,
        "foo.com"
      );
    });
    it("should dispatch once if the blocked is not a SPOC", async () => {
      Object.defineProperty(feed, "showSpocs", { get: () => true });
      sandbox.spy(feed.store, "dispatch");

      await feed.onAction({
        type: at.PLACES_LINK_BLOCKED,
        data: { url: "not_a_spoc.com" },
      });

      assert.calledOnce(feed.store.dispatch);
      assert.deepEqual(
        feed.store.dispatch.firstCall.args[0].data.url,
        "not_a_spoc.com"
      );
    });
    it("should dispatch a DISCOVERY_STREAM_SPOC_BLOCKED for a blocked spoc", async () => {
      Object.defineProperty(feed, "showSpocs", { get: () => true });
      sandbox.spy(feed.store, "dispatch");

      await feed.onAction({
        type: at.PLACES_LINK_BLOCKED,
        data: { url: "foo.com" },
      });

      assert.equal(
        feed.store.dispatch.secondCall.args[0].type,
        "DISCOVERY_STREAM_SPOC_BLOCKED"
      );
    });
  });

  describe("#onAction: BLOCK_URL", () => {
    it("should call recordBlockFlightId whith BLOCK_URL", async () => {
      sandbox.stub(feed, "recordBlockFlightId").returns();

      await feed.onAction({
        type: at.BLOCK_URL,
        data: [
          {
            flight_id: "1234",
          },
        ],
      });

      assert.calledWith(feed.recordBlockFlightId, "1234");
    });
  });

  describe("#onAction: INIT", () => {
    it("should be .loaded=false before initialization", () => {
      assert.isFalse(feed.loaded);
    });
    it("should load data and set .loaded=true if config.enabled is true", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      setPref(CONFIG_PREF_NAME, { enabled: true });
      sandbox.stub(feed, "loadLayout").returns(Promise.resolve());

      await feed.onAction({ type: at.INIT });

      assert.calledOnce(feed.loadLayout);
      assert.isTrue(feed.loaded);
    });
  });

  describe("#onAction: DISCOVERY_STREAM_CONFIG_SET_VALUE", async () => {
    it("should add the new value to the pref without changing the existing values", async () => {
      sandbox.spy(feed.store, "dispatch");
      setPref(CONFIG_PREF_NAME, { enabled: true, other: "value" });

      await feed.onAction({
        type: at.DISCOVERY_STREAM_CONFIG_SET_VALUE,
        data: { name: "layout_endpoint", value: "foo.com" },
      });

      assert.calledWithMatch(feed.store.dispatch, {
        data: {
          name: CONFIG_PREF_NAME,
          value: JSON.stringify({
            enabled: true,
            other: "value",
            layout_endpoint: "foo.com",
          }),
        },
        type: at.SET_PREF,
      });
    });
  });

  describe("#onAction: DISCOVERY_STREAM_POCKET_STATE_INIT", async () => {
    it("should call setupPocketState", async () => {
      sandbox.spy(feed, "setupPocketState");
      feed.onAction({
        type: at.DISCOVERY_STREAM_POCKET_STATE_INIT,
        meta: { fromTarget: {} },
      });
      assert.calledOnce(feed.setupPocketState);
    });
  });

  describe("#onAction: DISCOVERY_STREAM_CONFIG_RESET", async () => {
    it("should call configReset", async () => {
      sandbox.spy(feed, "configReset");
      feed.onAction({
        type: at.DISCOVERY_STREAM_CONFIG_RESET,
      });
      assert.calledOnce(feed.configReset);
    });
  });

  describe("#onAction: DISCOVERY_STREAM_CONFIG_RESET_DEFAULTS", async () => {
    it("Should dispatch CLEAR_PREF with pref name", async () => {
      sandbox.spy(feed.store, "dispatch");
      await feed.onAction({
        type: at.DISCOVERY_STREAM_CONFIG_RESET_DEFAULTS,
      });

      assert.calledWithMatch(feed.store.dispatch, {
        data: {
          name: CONFIG_PREF_NAME,
        },
        type: at.CLEAR_PREF,
      });
    });
  });

  describe("#onAction: DISCOVERY_STREAM_RETRY_FEED", async () => {
    it("should call retryFeed", async () => {
      sandbox.spy(feed, "retryFeed");
      feed.onAction({
        type: at.DISCOVERY_STREAM_RETRY_FEED,
        data: { feed: { url: "https://feed.com" } },
      });
      assert.calledOnce(feed.retryFeed);
      assert.calledWith(feed.retryFeed, { url: "https://feed.com" });
    });
  });

  describe("#onAction: DISCOVERY_STREAM_CONFIG_CHANGE", () => {
    it("should call this.loadLayout if config.enabled changes to true ", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      // First initialize
      await feed.onAction({ type: at.INIT });
      assert.isFalse(feed.loaded);

      // force clear cached pref value
      feed._prefCache = {};
      setPref(CONFIG_PREF_NAME, { enabled: true });

      sandbox.stub(feed, "resetCache").returns(Promise.resolve());
      sandbox.stub(feed, "loadLayout").returns(Promise.resolve());
      await feed.onAction({ type: at.DISCOVERY_STREAM_CONFIG_CHANGE });

      assert.calledOnce(feed.loadLayout);
      assert.calledOnce(feed.resetCache);
      assert.isTrue(feed.loaded);
    });
    it("should clear the cache if a config change happens and config.enabled is true", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      // force clear cached pref value
      feed._prefCache = {};
      setPref(CONFIG_PREF_NAME, { enabled: true });

      sandbox.stub(feed, "resetCache").returns(Promise.resolve());
      await feed.onAction({ type: at.DISCOVERY_STREAM_CONFIG_CHANGE });

      assert.calledOnce(feed.resetCache);
    });
    it("should dispatch DISCOVERY_STREAM_LAYOUT_RESET from DISCOVERY_STREAM_CONFIG_CHANGE", async () => {
      sandbox.stub(feed, "resetDataPrefs");
      sandbox.stub(feed, "resetCache").resolves();
      sandbox.stub(feed, "enable").resolves();
      setPref(CONFIG_PREF_NAME, { enabled: true });
      sandbox.spy(feed.store, "dispatch");

      await feed.onAction({ type: at.DISCOVERY_STREAM_CONFIG_CHANGE });

      assert.calledWithMatch(feed.store.dispatch, {
        type: at.DISCOVERY_STREAM_LAYOUT_RESET,
      });
    });
    it("should not call this.loadLayout if config.enabled changes to false", async () => {
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      // force clear cached pref value
      feed._prefCache = {};
      setPref(CONFIG_PREF_NAME, { enabled: true });

      await feed.onAction({ type: at.INIT });
      assert.isTrue(feed.loaded);

      feed._prefCache = {};
      setPref(CONFIG_PREF_NAME, { enabled: false });
      sandbox.stub(feed, "resetCache").returns(Promise.resolve());
      sandbox.stub(feed, "loadLayout").returns(Promise.resolve());
      await feed.onAction({ type: at.DISCOVERY_STREAM_CONFIG_CHANGE });

      assert.notCalled(feed.loadLayout);
      assert.calledOnce(feed.resetCache);
      assert.isFalse(feed.loaded);
    });
  });

  describe("#onAction: UNINIT", () => {
    it("should reset pref cache", async () => {
      feed._prefCache = { cached: "value" };

      await feed.onAction({ type: at.UNINIT });

      assert.deepEqual(feed._prefCache, {});
    });
  });

  describe("#onAction: PREF_CHANGED", () => {
    it("should update state.DiscoveryStream.config when the pref changes", async () => {
      setPref(CONFIG_PREF_NAME, {
        enabled: true,
        show_spocs: false,
        layout_endpoint: "foo",
      });

      assert.deepEqual(feed.store.getState().DiscoveryStream.config, {
        enabled: true,
        show_spocs: false,
        layout_endpoint: "foo",
      });
    });
    it("should fire loadSpocs is showSponsored pref changes", async () => {
      sandbox.stub(feed, "loadSpocs").returns(Promise.resolve());

      await feed.onAction({
        type: at.PREF_CHANGED,
        data: { name: "showSponsored" },
      });

      assert.calledOnce(feed.loadSpocs);
    });
    it("should fire onPocketConfigChanged when pocketConfig pref changes", async () => {
      sandbox.stub(feed, "onPocketConfigChanged").returns(Promise.resolve());

      await feed.onAction({
        type: at.PREF_CHANGED,
        data: { name: "pocketConfig", value: false },
      });

      assert.calledOnce(feed.onPocketConfigChanged);
    });
    it("should fire onPocketConfigChanged when collections pref changes", async () => {
      sandbox.stub(feed, "onPocketConfigChanged").returns(Promise.resolve());

      await feed.onAction({
        type: at.PREF_CHANGED,
        data: { name: "discoverystream.sponsored-collections.enabled" },
      });

      assert.calledOnce(feed.onPocketConfigChanged);
    });
    it("should call clearSpocs when sponsored content is turned off", async () => {
      sandbox.stub(feed, "clearSpocs").returns(Promise.resolve());

      await feed.onAction({
        type: at.PREF_CHANGED,
        data: { name: "showSponsored", value: false },
      });

      assert.calledOnce(feed.clearSpocs);
    });
    it("should call clearSpocs when top stories is turned off", async () => {
      sandbox.stub(feed, "clearSpocs").returns(Promise.resolve());

      await feed.onAction({
        type: at.PREF_CHANGED,
        data: { name: "feeds.section.topstories", value: false },
      });

      assert.calledOnce(feed.clearSpocs);
    });
    it("should re enable stories when top stories is turned on", async () => {
      sandbox.stub(feed, "refreshAll").returns(Promise.resolve());
      feed.loaded = true;
      setPref(CONFIG_PREF_NAME, {
        enabled: true,
      });

      await feed.onAction({
        type: at.PREF_CHANGED,
        data: { name: "feeds.section.topstories", value: true },
      });

      assert.calledOnce(feed.refreshAll);
    });
  });

  describe("#onAction: SYSTEM_TICK", () => {
    it("should not refresh if DiscoveryStream has not been loaded", async () => {
      sandbox.stub(feed, "refreshAll").resolves();
      setPref(CONFIG_PREF_NAME, { enabled: true });

      await feed.onAction({ type: at.SYSTEM_TICK });
      assert.notCalled(feed.refreshAll);
    });

    it("should not refresh if no caches are expired", async () => {
      sandbox.stub(feed.cache, "set").resolves();
      setPref(CONFIG_PREF_NAME, { enabled: true });

      await feed.onAction({ type: at.INIT });

      sandbox.stub(feed, "checkIfAnyCacheExpired").resolves(false);
      sandbox.stub(feed, "refreshAll").resolves();

      await feed.onAction({ type: at.SYSTEM_TICK });
      assert.notCalled(feed.refreshAll);
    });

    it("should refresh if DiscoveryStream has been loaded at least once and a cache has expired", async () => {
      sandbox.stub(feed.cache, "set").resolves();
      setPref(CONFIG_PREF_NAME, { enabled: true });

      await feed.onAction({ type: at.INIT });

      sandbox.stub(feed, "checkIfAnyCacheExpired").resolves(true);
      sandbox.stub(feed, "refreshAll").resolves();

      await feed.onAction({ type: at.SYSTEM_TICK });
      assert.calledOnce(feed.refreshAll);
    });

    it("should refresh and not update open tabs if DiscoveryStream has been loaded at least once", async () => {
      sandbox.stub(feed.cache, "set").resolves();
      setPref(CONFIG_PREF_NAME, { enabled: true });

      await feed.onAction({ type: at.INIT });

      sandbox.stub(feed, "checkIfAnyCacheExpired").resolves(true);
      sandbox.stub(feed, "refreshAll").resolves();

      await feed.onAction({ type: at.SYSTEM_TICK });
      assert.calledWith(feed.refreshAll, { updateOpenTabs: false });
    });
  });

  describe("#onPocketConfigChanged", () => {
    it("should call loadLayout when Pocket config changes", async () => {
      sandbox.stub(feed, "loadLayout").callsFake(dispatch => dispatch("foo"));
      sandbox.stub(feed.store, "dispatch");
      await feed.onPocketConfigChanged();
      assert.calledOnce(feed.loadLayout);
      assert.calledWith(feed.store.dispatch, ac.AlsoToPreloaded("foo"));
    });
  });

  describe("#onAction: PREF_SHOW_SPONSORED", () => {
    it("should call loadSpocs when preference changes", async () => {
      sandbox.stub(feed, "loadSpocs").resolves();
      sandbox.stub(feed.store, "dispatch");

      await feed.onAction({
        type: at.PREF_CHANGED,
        data: { name: "showSponsored" },
      });

      assert.calledOnce(feed.loadSpocs);
      const [dispatchFn] = feed.loadSpocs.firstCall.args;
      dispatchFn({});
      assert.calledWith(feed.store.dispatch, ac.BroadcastToContent({}));
    });
  });

  describe("#onAction: DISCOVERY_STREAM_DEV_IDLE_DAILY", () => {
    it("should trigger idle-daily observer", async () => {
      sandbox.stub(global.Services.obs, "notifyObservers").returns();
      await feed.onAction({
        type: at.DISCOVERY_STREAM_DEV_IDLE_DAILY,
      });
      assert.calledWith(
        global.Services.obs.notifyObservers,
        null,
        "idle-daily"
      );
    });
  });

  describe("#onAction: DISCOVERY_STREAM_DEV_SYNC_RS", () => {
    it("should fire remote settings pollChanges", async () => {
      sandbox.stub(global.RemoteSettings, "pollChanges").returns();
      await feed.onAction({
        type: at.DISCOVERY_STREAM_DEV_SYNC_RS,
      });
      assert.calledOnce(global.RemoteSettings.pollChanges);
    });
  });

  describe("#onAction: DISCOVERY_STREAM_DEV_SYSTEM_TICK", () => {
    it("should refresh if DiscoveryStream has been loaded at least once and a cache has expired", async () => {
      sandbox.stub(feed.cache, "set").resolves();
      setPref(CONFIG_PREF_NAME, { enabled: true });

      await feed.onAction({ type: at.INIT });

      sandbox.stub(feed, "checkIfAnyCacheExpired").resolves(true);
      sandbox.stub(feed, "refreshAll").resolves();

      await feed.onAction({ type: at.DISCOVERY_STREAM_DEV_SYSTEM_TICK });
      assert.calledOnce(feed.refreshAll);
    });
  });

  describe("#onAction: DISCOVERY_STREAM_DEV_EXPIRE_CACHE", () => {
    it("should fire resetCache", async () => {
      sandbox.stub(feed, "resetContentCache").returns();
      await feed.onAction({
        type: at.DISCOVERY_STREAM_DEV_EXPIRE_CACHE,
      });
      assert.calledOnce(feed.resetContentCache);
    });
  });

  describe("#isExpired", () => {
    it("should throw if the key is not valid", () => {
      assert.throws(() => {
        feed.isExpired({}, "foo");
      });
    });
    it("should return false for layout on startup for content under 1 week", () => {
      const layout = { lastUpdated: Date.now() };
      const result = feed.isExpired({
        cachedData: { layout },
        key: "layout",
        isStartup: true,
      });

      assert.isFalse(result);
    });
    it("should return true for layout for isStartup=false after 30 mins", () => {
      const layout = { lastUpdated: Date.now() };
      clock.tick(THIRTY_MINUTES + 1);
      const result = feed.isExpired({ cachedData: { layout }, key: "layout" });

      assert.isTrue(result);
    });
    it("should return true for layout on startup for content over 1 week", () => {
      const layout = { lastUpdated: Date.now() };
      clock.tick(ONE_WEEK + 1);
      const result = feed.isExpired({
        cachedData: { layout },
        key: "layout",
        isStartup: true,
      });

      assert.isTrue(result);
    });
    it("should return false for hardcoded layout on startup for content over 1 week", () => {
      feed._prefCache.config = {
        hardcoded_layout: true,
      };
      const layout = { lastUpdated: Date.now() };
      clock.tick(ONE_WEEK + 1);
      const result = feed.isExpired({
        cachedData: { layout },
        key: "layout",
        isStartup: true,
      });

      assert.isFalse(result);
    });
  });

  describe("#checkIfAnyCacheExpired", () => {
    let cache;
    beforeEach(() => {
      cache = {
        layout: { lastUpdated: Date.now() },
        feeds: { "foo.com": { lastUpdated: Date.now() } },
        spocs: { lastUpdated: Date.now() },
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
      cache.layout.lastUpdated = Date.now();
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
      cache.layout.lastUpdated = Date.now();
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
      Object.defineProperty(feed, "showSpocs", { get: () => true });
    });

    it("should call layout, component, spocs update and telemetry reporting functions", async () => {
      await feed.refreshAll();

      assert.calledOnce(feed.loadLayout);
      assert.calledOnce(feed.loadComponentFeeds);
      assert.calledOnce(feed.loadSpocs);
    });
    it("should pass in dispatch wrapped with broadcast if options.updateOpenTabs is true", async () => {
      await feed.refreshAll({ updateOpenTabs: true });
      [feed.loadLayout, feed.loadComponentFeeds, feed.loadSpocs].forEach(fn => {
        assert.calledOnce(fn);
        const result = fn.firstCall.args[0]({ type: "FOO" });
        assert.isTrue(au.isBroadcastToContent(result));
      });
    });
    it("should pass in dispatch with regular actions if options.updateOpenTabs is false", async () => {
      await feed.refreshAll({ updateOpenTabs: false });
      [feed.loadLayout, feed.loadComponentFeeds, feed.loadSpocs].forEach(fn => {
        assert.calledOnce(fn);
        const result = fn.firstCall.args[0]({ type: "FOO" });
        assert.deepEqual(result, { type: "FOO" });
      });
    });
    it("should set loaded to true if loadSpocs and loadComponentFeeds fails", async () => {
      feed.loadComponentFeeds.rejects("loadComponentFeeds error");
      feed.loadSpocs.rejects("loadSpocs error");

      await feed.enable();

      assert.isTrue(feed.loaded);
    });
    it("should call loadComponentFeeds and loadSpocs in Promise.all", async () => {
      sandbox.stub(global.Promise, "all").resolves();

      await feed.refreshAll();

      assert.calledOnce(global.Promise.all);
      const { args } = global.Promise.all.firstCall;
      assert.equal(args[0].length, 2);
    });
    describe("test startup cache behaviour", () => {
      beforeEach(() => {
        feed._maybeUpdateCachedData.restore();
        sandbox.stub(feed.cache, "set").resolves();
      });
      it("should refresh layout on startup if it was served from cache", async () => {
        feed.loadLayout.restore();
        sandbox
          .stub(feed.cache, "get")
          .resolves({ layout: { lastUpdated: Date.now(), layout: {} } });
        sandbox.stub(feed, "fetchFromEndpoint").resolves({ layout: {} });
        clock.tick(THIRTY_MINUTES + 1);

        await feed.refreshAll({ isStartup: true });

        assert.calledOnce(feed.fetchFromEndpoint);
        // Once from cache, once to update the store
        assert.calledTwice(feed.store.dispatch);
        assert.equal(
          feed.store.dispatch.firstCall.args[0].type,
          at.DISCOVERY_STREAM_LAYOUT_UPDATE
        );
      });
      it("should not refresh layout on startup if it is under THIRTY_MINUTES", async () => {
        feed.loadLayout.restore();
        sandbox
          .stub(feed.cache, "get")
          .resolves({ layout: { lastUpdated: Date.now(), layout: {} } });
        sandbox.stub(feed, "fetchFromEndpoint").resolves({ layout: {} });

        await feed.refreshAll({ isStartup: true });

        assert.notCalled(feed.fetchFromEndpoint);
      });
      it("should refresh spocs on startup if it was served from cache", async () => {
        feed.loadSpocs.restore();
        sandbox
          .stub(feed.cache, "get")
          .resolves({ spocs: { lastUpdated: Date.now() } });
        sandbox.stub(feed, "fetchFromEndpoint").resolves("data");
        clock.tick(THIRTY_MINUTES + 1);

        await feed.refreshAll({ isStartup: true });

        assert.calledOnce(feed.fetchFromEndpoint);
        // Once from cache, once to update the store
        assert.calledTwice(feed.store.dispatch);
        assert.equal(
          feed.store.dispatch.firstCall.args[0].type,
          at.DISCOVERY_STREAM_SPOCS_UPDATE
        );
      });
      it("should not refresh spocs on startup if it is under THIRTY_MINUTES", async () => {
        feed.loadSpocs.restore();
        sandbox
          .stub(feed.cache, "get")
          .resolves({ spocs: { lastUpdated: Date.now() } });
        sandbox.stub(feed, "fetchFromEndpoint").resolves("data");

        await feed.refreshAll({ isStartup: true });

        assert.notCalled(feed.fetchFromEndpoint);
      });
      it("should refresh feeds on startup if it was served from cache", async () => {
        feed.loadComponentFeeds.restore();

        const fakeComponents = { components: [{ feed: { url: "foo.com" } }] };
        const fakeLayout = [fakeComponents];
        const fakeDiscoveryStream = {
          DiscoveryStream: {
            layout: fakeLayout,
          },
          Prefs: {
            values: {
              "feeds.section.topstories": true,
              "feeds.system.topstories": true,
            },
          },
        };
        sandbox.stub(feed.store, "getState").returns(fakeDiscoveryStream);
        sandbox.stub(feed, "rotate").callsFake(val => val);
        sandbox
          .stub(feed, "scoreItems")
          .callsFake(val => ({ data: val, filtered: [] }));
        sandbox.stub(feed, "cleanUpTopRecImpressionPref").callsFake(val => val);

        const fakeCache = {
          feeds: { "foo.com": { lastUpdated: Date.now(), data: "data" } },
        };
        sandbox.stub(feed.cache, "get").resolves(fakeCache);
        clock.tick(THIRTY_MINUTES + 1);
        sandbox.stub(feed, "fetchFromEndpoint").resolves({
          recommendations: "data",
          settings: {
            recsExpireTime: 1,
          },
        });

        await feed.refreshAll({ isStartup: true });

        assert.calledOnce(feed.fetchFromEndpoint);
        // Once from cache, once to update the feed, once to update that all feeds are done.
        assert.calledThrice(feed.store.dispatch);
        assert.equal(
          feed.store.dispatch.secondCall.args[0].type,
          at.DISCOVERY_STREAM_FEEDS_UPDATE
        );
      });
    });
  });

  describe("#scoreFeeds", () => {
    it("should score feeds and set cache, and dispatch", async () => {
      sandbox.stub(feed.cache, "set").resolves();
      sandbox.spy(feed.store, "dispatch");
      const recsExpireTime = 5600;
      const fakeImpressions = {
        first: Date.now() - recsExpireTime * 1000,
        third: Date.now(),
      };
      sandbox.stub(feed, "readDataPref").returns(fakeImpressions);
      const fakeFeeds = {
        data: {
          "https://foo.com": {
            data: {
              recommendations: [
                {
                  id: "first",
                  item_score: 0.7,
                },
                {
                  id: "second",
                  item_score: 0.6,
                },
              ],
              settings: {
                recsExpireTime,
              },
            },
          },
          "https://bar.com": {
            data: {
              recommendations: [
                {
                  id: "third",
                  item_score: 0.4,
                },
                {
                  id: "fourth",
                  item_score: 0.6,
                },
                {
                  id: "fifth",
                  item_score: 0.8,
                },
              ],
              settings: {
                recsExpireTime,
              },
            },
          },
        },
      };
      const feedsTestResult = {
        "https://foo.com": {
          data: {
            recommendations: [
              {
                id: "second",
                item_score: 0.6,
                score: 0.6,
              },
              {
                id: "first",
                item_score: 0.7,
                score: 0.7,
              },
            ],
            settings: {
              recsExpireTime,
            },
          },
        },
        "https://bar.com": {
          data: {
            recommendations: [
              {
                id: "fifth",
                item_score: 0.8,
                score: 0.8,
              },
              {
                id: "fourth",
                item_score: 0.6,
                score: 0.6,
              },
              {
                id: "third",
                item_score: 0.4,
                score: 0.4,
              },
            ],
            settings: {
              recsExpireTime,
            },
          },
        },
      };

      await feed.scoreFeeds(fakeFeeds);

      assert.calledWith(feed.cache.set, "feeds", feedsTestResult);
      assert.equal(
        feed.store.dispatch.firstCall.args[0].type,
        at.DISCOVERY_STREAM_FEED_UPDATE
      );
      assert.deepEqual(feed.store.dispatch.firstCall.args[0].data, {
        url: "https://foo.com",
        feed: feedsTestResult["https://foo.com"],
      });
      assert.equal(
        feed.store.dispatch.secondCall.args[0].type,
        at.DISCOVERY_STREAM_FEED_UPDATE
      );
      assert.deepEqual(feed.store.dispatch.secondCall.args[0].data, {
        url: "https://bar.com",
        feed: feedsTestResult["https://bar.com"],
      });
    });
  });

  describe("#scoreSpocs", () => {
    it("should score spocs and set cache, dispatch", async () => {
      sandbox.stub(feed.cache, "set").resolves();
      sandbox.spy(feed.store, "dispatch");
      const fakeDiscoveryStream = {
        Prefs: {
          values: {
            "discoverystream.spocs.personalized": true,
            "discoverystream.recs.personalized": false,
          },
        },
        DiscoveryStream: {
          spocs: {
            placements: [
              { name: "placement1" },
              { name: "placement2" },
              { name: "placement3" },
            ],
          },
        },
      };
      sandbox.stub(feed.store, "getState").returns(fakeDiscoveryStream);
      const fakeSpocs = {
        lastUpdated: 1234,
        data: {
          placement1: {
            items: [
              {
                item_score: 0.6,
              },
              {
                item_score: 0.4,
              },
              {
                item_score: 0.8,
              },
            ],
          },
          placement2: {
            items: [
              {
                item_score: 0.6,
              },
              {
                item_score: 0.8,
              },
            ],
          },
          placement3: { items: [] },
        },
      };

      await feed.scoreSpocs(fakeSpocs);

      const spocsTestResult = {
        lastUpdated: 1234,
        spocs: {
          placement1: {
            items: [
              {
                score: 0.8,
                item_score: 0.8,
              },
              {
                score: 0.6,
                item_score: 0.6,
              },
              {
                score: 0.4,
                item_score: 0.4,
              },
            ],
          },
          placement2: {
            items: [
              {
                score: 0.8,
                item_score: 0.8,
              },
              {
                score: 0.6,
                item_score: 0.6,
              },
            ],
          },
          placement3: { items: [] },
        },
      };
      assert.calledWith(feed.cache.set, "spocs", spocsTestResult);
      assert.equal(
        feed.store.dispatch.firstCall.args[0].type,
        at.DISCOVERY_STREAM_SPOCS_UPDATE
      );
      assert.deepEqual(
        feed.store.dispatch.firstCall.args[0].data,
        spocsTestResult
      );
    });
  });

  describe("#scoreContent", () => {
    it("should call scoreFeeds and scoreSpocs if loaded", async () => {
      const fakeDiscoveryStream = {
        Prefs: {
          values: {
            pocketConfig: {
              recsPersonalized: true,
              spocsPersonalized: true,
            },
            "discoverystream.personalization.enabled": true,
            "feeds.section.topstories": true,
            "feeds.system.topstories": true,
          },
        },
        DiscoveryStream: {
          feeds: { loaded: false },
          spocs: { loaded: false },
        },
      };

      sandbox.stub(feed, "scoreFeeds").resolves();
      sandbox.stub(feed, "scoreSpocs").resolves();
      sandbox.stub(feed, "refreshContent").resolves();
      sandbox.stub(feed, "loadPersonalizationScoresCache").resolves();
      sandbox.stub(feed.store, "getState").returns(fakeDiscoveryStream);
      sandbox.stub(feed, "_checkExpirationPerComponent").resolves({
        feeds: true,
        spocs: true,
      });

      await feed.refreshAll();

      assert.notCalled(feed.scoreFeeds);
      assert.notCalled(feed.scoreSpocs);

      fakeDiscoveryStream.DiscoveryStream.feeds.loaded = true;
      fakeDiscoveryStream.DiscoveryStream.spocs.loaded = true;

      await feed.refreshAll();

      assert.calledOnce(feed.scoreFeeds);
      assert.calledOnce(feed.scoreSpocs);
    });
  });

  describe("#loadPersonalizationScoresCache", () => {
    it("should create a personalization provider from cached scores", async () => {
      feed.store.getState = () => ({
        Prefs: {
          values: {
            pocketConfig: {
              recsPersonalized: true,
              spocsPersonalized: true,
            },
            "discoverystream.personalization.enabled": true,
            "feeds.section.topstories": true,
            "feeds.system.topstories": true,
          },
        },
      });
      const fakeCache = {
        personalization: {
          scores: 123,
          _timestamp: 456,
        },
      };
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));

      await feed.loadPersonalizationScoresCache();

      assert.equal(feed.personalizationLastUpdated, 456);
    });
  });

  describe("#observe", () => {
    it("should call updatePersonalizationScores on idle daily", async () => {
      sandbox.stub(feed, "updatePersonalizationScores").returns();
      feed.observe(null, "idle-daily");
      assert.calledOnce(feed.updatePersonalizationScores);
    });
    it("should call configReset on Pocket button pref change", async () => {
      sandbox.stub(feed, "configReset").returns();
      feed.observe(null, "nsPref:changed", "extensions.pocket.enabled");
      assert.calledOnce(feed.configReset);
    });
  });

  describe("#updatePersonalizationScores", () => {
    it("should update recommendationProvider on updatePersonalizationScores", async () => {
      feed.store.getState = () => ({
        Prefs: {
          values: {
            pocketConfig: {
              recsPersonalized: true,
              spocsPersonalized: true,
            },
            "discoverystream.personalization.enabled": true,
            "feeds.section.topstories": true,
            "feeds.system.topstories": true,
          },
        },
      });
      sandbox.stub(feed.recommendationProvider, "init").returns();

      await feed.updatePersonalizationScores();

      assert.deepEqual(feed.recommendationProvider.provider.getScores(), {
        interestConfig: undefined,
        interestVector: undefined,
      });
    });
    it("should not update recommendationProvider on updatePersonalizationScores", async () => {
      feed.store.getState = () => ({
        Prefs: {
          values: {
            "discoverystream.spocs.personalized": true,
            "discoverystream.recs.personalized": true,
            "discoverystream.personalization.enabled": false,
          },
        },
      });
      await feed.updatePersonalizationScores();

      assert.isTrue(!feed.recommendationProvider.provider);
    });
  });
  describe("#scoreItem", () => {
    it("should call calculateItemRelevanceScore with recommendationProvider with initial score", async () => {
      const item = {
        item_score: 0.6,
      };
      feed.store.getState = () => ({
        Prefs: {
          values: {
            pocketConfig: {
              recsPersonalized: true,
              spocsPersonalized: true,
            },
            "discoverystream.personalization.enabled": true,
            "feeds.section.topstories": true,
            "feeds.system.topstories": true,
          },
        },
      });
      feed.recommendationProvider.calculateItemRelevanceScore = sandbox
        .stub()
        .returns();
      const result = await feed.scoreItem(item, true);
      assert.calledOnce(
        feed.recommendationProvider.calculateItemRelevanceScore
      );
      assert.equal(result.score, 0.6);
    });
    it("should fallback to score 1 without an initial score", async () => {
      const item = {};
      feed.store.getState = () => ({
        Prefs: {
          values: {
            "discoverystream.spocs.personalized": true,
            "discoverystream.recs.personalized": true,
            "discoverystream.personalization.enabled": true,
          },
        },
      });
      feed.recommendationProvider.calculateItemRelevanceScore = sandbox
        .stub()
        .returns();
      const result = await feed.scoreItem(item, true);
      assert.equal(result.score, 1);
    });
  });
});
