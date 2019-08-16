import {
  actionCreators as ac,
  actionTypes as at,
  actionUtils as au,
} from "common/Actions.jsm";
import { combineReducers, createStore } from "redux";
import { GlobalOverrider } from "test/unit/utils";
import injector from "inject!lib/DiscoveryStreamFeed.jsm";
import { reducers } from "common/Reducers.jsm";

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
  let DiscoveryStreamFeed;
  let feed;
  let sandbox;
  let fetchStub;
  let clock;
  let fakeNewTabUtils;
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

    class FakeUserDomainAffinityProvider {
      constructor(
        timeSegments,
        parameterSets,
        maxHistoryQueryResults,
        version,
        scores
      ) {
        this.timeSegments = timeSegments;
        this.parameterSets = parameterSets;
        this.maxHistoryQueryResults = maxHistoryQueryResults;
        this.version = version;
        this.scores = scores;
      }

      getAffinities() {
        return {};
      }
    }

    // Fetch
    fetchStub = sandbox.stub(global, "fetch");

    // Time
    clock = sinon.useFakeTimers();

    // Injector
    ({ DiscoveryStreamFeed } = injector({
      "lib/UserDomainAffinityProvider.jsm": {
        UserDomainAffinityProvider: FakeUserDomainAffinityProvider,
      },
    }));

    globals = new GlobalOverrider();
    globals.set("gUUIDGenerator", { generateUUID: () => FAKE_UUID });

    sandbox
      .stub(global.Services.prefs, "getBoolPref")
      .withArgs("browser.newtabpage.activity-stream.discoverystream.enabled")
      .returns(true);

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
        },
      },
    });
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
      });
      assert.calledWith(feed.store.dispatch.secondCall, {
        type: at.DISCOVERY_STREAM_FEEDS_UPDATE,
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

    it("should not record the request time if no fetch request was issued", async () => {
      const fakeComponents = { components: [] };
      const fakeLayout = [fakeComponents, { components: [{}] }, {}];
      fakeDiscoveryStream = { DiscoveryStream: { layout: fakeLayout } };
      fakeCache = {
        feeds: { "foo.com": { lastUpdated: Date.now(), data: "data" } },
      };
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));
      feed.componentFeedRequestTime = undefined;

      await feed.loadComponentFeeds(feed.store.dispatch);

      assert.isUndefined(feed.componentFeedRequestTime);
    });
  });

  describe("#getComponentFeed", () => {
    it("should fetch fresh data if cache is empty", async () => {
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
    it("should fetch fresh data if cache is old", async () => {
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
    it("should return data from cache if it is fresh", async () => {
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
    it("should fetch fresh data if cache is empty", async () => {
      sandbox.stub(feed.cache, "get").returns(Promise.resolve());
      sandbox.stub(feed, "fetchFromEndpoint").resolves("data");
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());

      await feed.loadSpocs(feed.store.dispatch);

      assert.calledWith(feed.cache.set, "spocs", {
        data: "data",
        lastUpdated: 0,
      });
      assert.equal(feed.store.getState().DiscoveryStream.spocs.data, "data");
    });
    it("should fetch fresh data if cache is old", async () => {
      const cachedSpoc = { data: "old", lastUpdated: Date.now() };
      const cachedData = { spocs: cachedSpoc };
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(cachedData));
      sandbox.stub(feed, "fetchFromEndpoint").resolves("new");
      sandbox.stub(feed.cache, "set").returns(Promise.resolve());
      clock.tick(THIRTY_MINUTES + 1);

      await feed.loadSpocs(feed.store.dispatch);

      assert.equal(feed.store.getState().DiscoveryStream.spocs.data, "new");
    });
    it("should return data from cache if it is fresh", async () => {
      const cachedSpoc = { data: "old", lastUpdated: Date.now() };
      const cachedData = { spocs: cachedSpoc };
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
      sandbox.stub(feed, "readImpressionsPref").returns(fakeImpressions);

      const result = feed.rotate(
        feedResponse.recommendations,
        feedResponse.settings.recsExpireTime
      );

      assert.equal(result[3].id, "first");
    });
  });

  describe("#resetCache", () => {
    it("should set .layout, .feeds .spocs and .affinities to {", async () => {
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
      assert.deepEqual(fourthCall.args, ["affinities", {}]);
    });
  });

  describe("#transform", () => {
    it("should return initial data if spocs are empty", () => {
      const { data: result } = feed.transform({ spocs: [] });

      assert.equal(result.spocs.length, 0);
    });
    it("should sort based on item_score", () => {
      const { data: result } = feed.transform({
        spocs: [
          { id: 2, campaign_id: 2, item_score: 0.8, min_score: 0.1 },
          { id: 3, campaign_id: 3, item_score: 0.7, min_score: 0.1 },
          { id: 1, campaign_id: 1, item_score: 0.9, min_score: 0.1 },
        ],
      });

      assert.deepEqual(result.spocs, [
        { id: 1, campaign_id: 1, item_score: 0.9, score: 0.9, min_score: 0.1 },
        { id: 2, campaign_id: 2, item_score: 0.8, score: 0.8, min_score: 0.1 },
        { id: 3, campaign_id: 3, item_score: 0.7, score: 0.7, min_score: 0.1 },
      ]);
    });
    it("should remove items with scores lower than min_score", () => {
      const { data: result, filtered } = feed.transform({
        spocs: [
          { id: 2, campaign_id: 2, item_score: 0.8, min_score: 0.9 },
          { id: 3, campaign_id: 3, item_score: 0.7, min_score: 0.7 },
          { id: 1, campaign_id: 1, item_score: 0.9, min_score: 0.8 },
        ],
      });

      assert.deepEqual(result.spocs, [
        { id: 1, campaign_id: 1, item_score: 0.9, score: 0.9, min_score: 0.8 },
        { id: 3, campaign_id: 3, item_score: 0.7, score: 0.7, min_score: 0.7 },
      ]);

      assert.deepEqual(filtered.below_min_score, [
        { id: 2, campaign_id: 2, item_score: 0.8, min_score: 0.9, score: 0.8 },
      ]);
    });
    it("should add a score prop to spocs", () => {
      const { data: result } = feed.transform({
        spocs: [{ campaign_id: 1, item_score: 0.9, min_score: 0.1 }],
      });

      assert.equal(result.spocs[0].score, 0.9);
    });
    it("should filter out duplicate campigns", () => {
      const { data: result, filtered } = feed.transform({
        spocs: [
          { id: 1, campaign_id: 2, item_score: 0.8, min_score: 0.1 },
          { id: 2, campaign_id: 3, item_score: 0.6, min_score: 0.1 },
          { id: 3, campaign_id: 1, item_score: 0.9, min_score: 0.1 },
          { id: 4, campaign_id: 3, item_score: 0.7, min_score: 0.1 },
          { id: 5, campaign_id: 1, item_score: 0.9, min_score: 0.1 },
        ],
      });

      assert.deepEqual(result.spocs, [
        { id: 3, campaign_id: 1, item_score: 0.9, score: 0.9, min_score: 0.1 },
        { id: 1, campaign_id: 2, item_score: 0.8, score: 0.8, min_score: 0.1 },
        { id: 4, campaign_id: 3, item_score: 0.7, score: 0.7, min_score: 0.1 },
      ]);

      assert.deepEqual(filtered.campaign_duplicate, [
        { id: 5, campaign_id: 1, item_score: 0.9, min_score: 0.1, score: 0.9 },
        { id: 2, campaign_id: 3, item_score: 0.6, min_score: 0.1, score: 0.6 },
      ]);
    });
    it("should filter out duplicate campigns while using spocs_per_domain", () => {
      sandbox.stub(feed.store, "getState").returns({
        DiscoveryStream: {
          spocs: { spocs_per_domain: 2 },
        },
      });

      const { data: result, filtered } = feed.transform({
        spocs: [
          { id: 1, campaign_id: 2, item_score: 0.8, min_score: 0.1 },
          { id: 2, campaign_id: 3, item_score: 0.6, min_score: 0.1 },
          { id: 3, campaign_id: 1, item_score: 0.6, min_score: 0.1 },
          { id: 4, campaign_id: 3, item_score: 0.7, min_score: 0.1 },
          { id: 5, campaign_id: 1, item_score: 0.9, min_score: 0.1 },
          { id: 6, campaign_id: 2, item_score: 0.6, min_score: 0.1 },
          { id: 7, campaign_id: 3, item_score: 0.7, min_score: 0.1 },
          { id: 8, campaign_id: 1, item_score: 0.8, min_score: 0.1 },
          { id: 9, campaign_id: 3, item_score: 0.7, min_score: 0.1 },
          { id: 10, campaign_id: 1, item_score: 0.8, min_score: 0.1 },
        ],
      });

      assert.deepEqual(result.spocs, [
        { id: 5, campaign_id: 1, item_score: 0.9, score: 0.9, min_score: 0.1 },
        { id: 1, campaign_id: 2, item_score: 0.8, score: 0.8, min_score: 0.1 },
        { id: 8, campaign_id: 1, item_score: 0.8, score: 0.8, min_score: 0.1 },
        { id: 4, campaign_id: 3, item_score: 0.7, score: 0.7, min_score: 0.1 },
        { id: 7, campaign_id: 3, item_score: 0.7, score: 0.7, min_score: 0.1 },
        { id: 6, campaign_id: 2, item_score: 0.6, score: 0.6, min_score: 0.1 },
      ]);

      assert.deepEqual(filtered.campaign_duplicate, [
        { id: 10, campaign_id: 1, item_score: 0.8, min_score: 0.1, score: 0.8 },
        { id: 9, campaign_id: 3, item_score: 0.7, min_score: 0.1, score: 0.7 },
        { id: 2, campaign_id: 3, item_score: 0.6, min_score: 0.1, score: 0.6 },
        { id: 3, campaign_id: 1, item_score: 0.6, min_score: 0.1, score: 0.6 },
      ]);
    });
  });

  describe("#filterBlocked", () => {
    it("should return initial data if spocs are empty", () => {
      const { data: result } = feed.filterBlocked({ spocs: [] });

      assert.equal(result.spocs.length, 0);
    });
    it("should return initial spocs data if links are not blocked", () => {
      const { data: result } = feed.filterBlocked(
        {
          spocs: [{ url: "https://foo.com" }, { url: "test.com" }],
        },
        "spocs"
      );
      assert.equal(result.spocs.length, 2);
    });
    it("should return filtered out spocs based on blockedlist", () => {
      fakeNewTabUtils.blockedLinks.links = [{ url: "https://foo.com" }];
      fakeNewTabUtils.blockedLinks.isBlocked = site =>
        fakeNewTabUtils.blockedLinks.links[0].url === site.url;

      const { data: result, filtered } = feed.filterBlocked(
        {
          spocs: [
            { id: 1, url: "https://foo.com" },
            { id: 2, url: "test.com" },
          ],
        },
        "spocs"
      );

      assert.lengthOf(result.spocs, 1);
      assert.equal(result.spocs[0].url, "test.com");
      assert.notInclude(result.spocs, fakeNewTabUtils.blockedLinks.links[0]);
      assert.deepEqual(filtered, [{ id: 1, url: "https://foo.com" }]);
    });
    it("should return initial recommendations data if links are not blocked", () => {
      const { data: result } = feed.filterBlocked(
        {
          recommendations: [{ url: "https://foo.com" }, { url: "test.com" }],
        },
        "recommendations"
      );
      assert.equal(result.recommendations.length, 2);
    });
    it("should return filtered out recommendations based on blockedlist", () => {
      fakeNewTabUtils.blockedLinks.links = [{ url: "https://foo.com" }];
      fakeNewTabUtils.blockedLinks.isBlocked = site =>
        fakeNewTabUtils.blockedLinks.links[0].url === site.url;

      const { data: result } = feed.filterBlocked(
        {
          recommendations: [{ url: "https://foo.com" }, { url: "test.com" }],
        },
        "recommendations"
      );

      assert.lengthOf(result.recommendations, 1);
      assert.equal(result.recommendations[0].url, "test.com");
      assert.notInclude(
        result.recommendations,
        fakeNewTabUtils.blockedLinks.links[0]
      );
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
      const fakeSpocs = {
        spocs: [
          {
            id: 1,
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
            id: 2,
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
        seen: [Date.now() - 1],
      };
      sandbox.stub(feed, "readImpressionsPref").returns(fakeImpressions);

      const { data: result, filtered } = feed.frequencyCapSpocs(fakeSpocs);

      assert.equal(result.spocs.length, 1);
      assert.equal(result.spocs[0].campaign_id, "not-seen");
      assert.deepEqual(filtered, [fakeSpocs.spocs[0]]);
    });
  });

  describe("#isBelowFrequencyCap", () => {
    it("should return true if there are no campaign impressions", () => {
      const fakeImpressions = {
        seen: [Date.now() - 1],
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
        seen: [Date.now() - 1],
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
        seen: [Date.now() - 1],
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
        seen: [Date.now() - 1],
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

  describe("#recordCampaignImpression", () => {
    it("should return false if time based cap is hit", () => {
      sandbox.stub(feed, "readImpressionsPref").returns({});
      sandbox.stub(feed, "writeImpressionsPref").returns();

      feed.recordCampaignImpression("seen");

      assert.calledWith(
        feed.writeImpressionsPref,
        SPOC_IMPRESSION_TRACKING_PREF,
        { seen: [0] }
      );
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

      assert.calledWith(
        feed.writeImpressionsPref,
        SPOC_IMPRESSION_TRACKING_PREF,
        { "campaign-2": [-1] }
      );
    });
  });

  describe("#recordTopRecImpressions", () => {
    it("should add a rec id to the rec impression pref", () => {
      sandbox.stub(feed, "readImpressionsPref").returns({});
      sandbox.stub(feed, "writeImpressionsPref");

      feed.recordTopRecImpressions("rec");

      assert.calledWith(
        feed.writeImpressionsPref,
        REC_IMPRESSION_TRACKING_PREF,
        { rec: 0 }
      );
    });
    it("should not add an impression if it already exists", () => {
      sandbox.stub(feed, "readImpressionsPref").returns({ rec: 4 });
      sandbox.stub(feed, "writeImpressionsPref");

      feed.recordTopRecImpressions("rec");

      assert.notCalled(feed.writeImpressionsPref);
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
      sandbox.stub(feed, "readImpressionsPref").returns(fakeImpressions);
      sandbox.stub(feed, "writeImpressionsPref").returns();

      feed.cleanUpTopRecImpressionPref(newFeeds);

      assert.calledWith(
        feed.writeImpressionsPref,
        REC_IMPRESSION_TRACKING_PREF,
        { rec2: -1, rec3: -1 }
      );
    });
  });

  describe("#writeImpressionsPref", () => {
    it("should call Services.prefs.setStringPref", () => {
      sandbox.spy(feed.store, "dispatch");
      const fakeImpressions = {
        foo: [Date.now() - 1],
        bar: [Date.now() - 1],
      };

      feed.writeImpressionsPref(SPOC_IMPRESSION_TRACKING_PREF, fakeImpressions);

      assert.calledWithMatch(feed.store.dispatch, {
        data: {
          name: SPOC_IMPRESSION_TRACKING_PREF,
          value: JSON.stringify(fakeImpressions),
        },
        type: at.SET_PREF,
      });
    });
  });

  describe("#readImpressionsPref", () => {
    it("should return what's in Services.prefs.getStringPref", () => {
      const fakeImpressions = {
        foo: [Date.now() - 1],
        bar: [Date.now() - 1],
      };
      setPref(SPOC_IMPRESSION_TRACKING_PREF, fakeImpressions);

      const result = feed.readImpressionsPref(SPOC_IMPRESSION_TRACKING_PREF);

      assert.deepEqual(result, fakeImpressions);
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
        spocs: [
          {
            id: 1,
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
            id: 2,
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
      sandbox.stub(feed.store, "getState").returns({
        DiscoveryStream: {
          spocs: {
            data,
            spocs_per_domain: 2,
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
        spocs: [
          {
            id: 2,
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
      const spocFillResult = [
        {
          id: 1,
          reason: "frequency_cap",
          displayed: 0,
          full_recalc: 0,
        },
      ];

      sandbox.stub(feed, "recordCampaignImpression").returns();
      sandbox.stub(feed, "readImpressionsPref").returns(fakeImpressions);
      sandbox.spy(feed.store, "dispatch");

      await feed.onAction({
        type: at.DISCOVERY_STREAM_SPOC_IMPRESSION,
        data: { campaign_id: "seen" },
      });

      assert.deepEqual(
        feed.store.dispatch.secondCall.args[0].data.spocs,
        result
      );
      assert.deepEqual(
        feed.store.dispatch.thirdCall.args[0].data.spoc_fills,
        spocFillResult
      );
    });
    it("should not call dispatch to ac.AlsoToPreloaded if spocs were not changed by frequency capping", async () => {
      Object.defineProperty(feed, "showSpocs", { get: () => true });
      const fakeImpressions = {};
      sandbox.stub(feed, "recordCampaignImpression").returns();
      sandbox.stub(feed, "readImpressionsPref").returns(fakeImpressions);
      sandbox.spy(feed.store, "dispatch");

      await feed.onAction({
        type: at.DISCOVERY_STREAM_SPOC_IMPRESSION,
        data: { campaign_id: "seen" },
      });

      assert.notCalled(feed.store.dispatch);
    });
  });

  describe("#onAction: PLACES_LINK_BLOCKED", () => {
    beforeEach(() => {
      const data = {
        spocs: [
          {
            id: 1,
            campaign_id: "foo",
            url: "foo.com",
          },
          {
            id: 2,
            campaign_id: "bar",
            url: "bar.com",
          },
        ],
      };
      sandbox.stub(feed.store, "getState").returns({
        DiscoveryStream: {
          spocs: { data },
        },
      });
    });

    it("should call dispatch with the SPOCS Fill if found a blocked spoc", async () => {
      Object.defineProperty(feed, "showSpocs", { get: () => true });
      const spocFillResult = [
        {
          id: 1,
          reason: "blocked_by_user",
          displayed: 0,
          full_recalc: 0,
        },
      ];

      sandbox.spy(feed.store, "dispatch");

      await feed.onAction({
        type: at.PLACES_LINK_BLOCKED,
        data: { url: "foo.com" },
      });

      assert.deepEqual(
        feed.store.dispatch.firstCall.args[0].data.spoc_fills,
        spocFillResult
      );
      assert.deepEqual(
        feed.store.dispatch.secondCall.args[0].data.url,
        "foo.com"
      );
    });
    it("should not call dispatch with the SPOCS Fill if the blocked is not a SPOC", async () => {
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
        feed.store.dispatch.thirdCall.args[0].type,
        "DISCOVERY_STREAM_SPOC_BLOCKED"
      );
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
      sandbox.stub(feed, "reportCacheAge").resolves();
      sandbox.spy(feed, "reportRequestTime");

      await feed.onAction({ type: at.INIT });

      assert.calledOnce(feed.loadLayout);
      assert.calledOnce(feed.reportCacheAge);
      assert.calledOnce(feed.reportRequestTime);
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
      sandbox.stub(feed, "resetImpressionPrefs");
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
        const fakeDiscoveryStream = { DiscoveryStream: { layout: fakeLayout } };
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

  describe("#reportCacheAge", () => {
    let cache;
    const cacheAge = 30;
    beforeEach(() => {
      cache = {
        layout: { lastUpdated: Date.now() - 10 * 1000 },
        feeds: { "foo.com": { lastUpdated: Date.now() - cacheAge * 1000 } },
        spocs: { lastUpdated: Date.now() - 20 * 1000 },
      };
      sandbox.stub(feed.cache, "get").resolves(cache);
    });

    it("should report the oldest lastUpdated date as the cache age", async () => {
      sandbox.spy(feed.store, "dispatch");
      feed.loaded = false;
      await feed.reportCacheAge();

      assert.calledOnce(feed.store.dispatch);

      const [action] = feed.store.dispatch.firstCall.args;
      assert.equal(action.type, at.TELEMETRY_PERFORMANCE_EVENT);
      assert.equal(action.data.event, "DS_CACHE_AGE_IN_SEC");
      assert.isAtLeast(action.data.value, cacheAge);
      feed.loaded = true;
    });
  });

  describe("#reportRequestTime", () => {
    let cache;
    const cacheAge = 30;
    beforeEach(() => {
      cache = {
        layout: { lastUpdated: Date.now() - 10 * 1000 },
        feeds: { "foo.com": { lastUpdated: Date.now() - cacheAge * 1000 } },
        spocs: { lastUpdated: Date.now() - 20 * 1000 },
      };
      sandbox.stub(feed.cache, "get").resolves(cache);
    });

    it("should report all the request times", async () => {
      sandbox.spy(feed.store, "dispatch");
      feed.loaded = false;
      feed.layoutRequestTime = 1000;
      feed.spocsRequestTime = 2000;
      feed.componentFeedRequestTime = 3000;
      feed.totalRequestTime = 5000;
      feed.reportRequestTime();

      assert.equal(feed.store.dispatch.callCount, 4);

      let [action] = feed.store.dispatch.getCall(0).args;
      assert.equal(action.type, at.TELEMETRY_PERFORMANCE_EVENT);
      assert.equal(action.data.event, "LAYOUT_REQUEST_TIME");
      assert.equal(action.data.value, 1000);

      [action] = feed.store.dispatch.getCall(1).args;
      assert.equal(action.type, at.TELEMETRY_PERFORMANCE_EVENT);
      assert.equal(action.data.event, "SPOCS_REQUEST_TIME");
      assert.equal(action.data.value, 2000);

      [action] = feed.store.dispatch.getCall(2).args;
      assert.equal(action.type, at.TELEMETRY_PERFORMANCE_EVENT);
      assert.equal(action.data.event, "COMPONENT_FEED_REQUEST_TIME");
      assert.equal(action.data.value, 3000);

      [action] = feed.store.dispatch.getCall(3).args;
      assert.equal(action.type, at.TELEMETRY_PERFORMANCE_EVENT);
      assert.equal(action.data.event, "DS_FEED_TOTAL_REQUEST_TIME");
      assert.equal(action.data.value, 5000);
      feed.loaded = true;
    });
  });

  describe("#loadAffinityScoresCache", () => {
    it("should create an affinity provider from cached affinities", async () => {
      feed._prefCache.config = {
        personalized: true,
      };
      const fakeCache = {
        affinities: {
          scores: 123,
          _timestamp: 456,
        },
      };
      sandbox.stub(feed.cache, "get").returns(Promise.resolve(fakeCache));

      await feed.loadAffinityScoresCache();

      assert.equal(feed.domainAffinitiesLastUpdated, 456);
    });
  });

  describe("#updateDomainAffinityScores", () => {
    it("should update affinity provider on idle daily", async () => {
      feed._prefCache.config = {
        personalized: true,
      };
      feed.affinities = {
        parameterSets: {
          default: {},
        },
        maxHistoryQueryResults: 1000,
        timeSegments: [],
        version: "123",
      };

      feed.observe(null, "idle-daily");

      assert.equal(feed.affinityProvider.version, "123");
    });
    it("should not update affinity provider on idle daily", async () => {
      feed._prefCache.config = {
        personalized: false,
      };

      feed.observe(null, "idle-daily");

      assert.isTrue(!feed.affinityProvider);
    });
  });
  describe("#scoreItems", () => {
    it("should score items using item_score and min_score", () => {
      const { data: result, filtered } = feed.scoreItems([
        { item_score: 0.8, min_score: 0.1 },
        { item_score: 0.5, min_score: 0.6 },
        { item_score: 0.7, min_score: 0.1 },
        { item_score: 0.9, min_score: 0.1 },
      ]);
      assert.deepEqual(result, [
        { item_score: 0.9, score: 0.9, min_score: 0.1 },
        { item_score: 0.8, score: 0.8, min_score: 0.1 },
        { item_score: 0.7, score: 0.7, min_score: 0.1 },
      ]);
      assert.deepEqual(filtered, [
        { item_score: 0.5, min_score: 0.6, score: 0.5 },
      ]);
    });
  });

  describe("#scoreItem", () => {
    it("should use personalized score with affinity provider", () => {
      const item = {};
      feed._prefCache.config = {
        personalized: true,
      };
      feed.affinityProvider = {
        calculateItemRelevanceScore: () => 0.5,
      };
      const result = feed.scoreItem(item);
      assert.equal(result.score, 0.5);
    });
    it("should use item_score score without affinity provider score", () => {
      const item = {
        item_score: 0.6,
      };
      feed._prefCache.config = {
        personalized: true,
      };
      feed.affinityProvider = {
        calculateItemRelevanceScore: () => {},
      };
      const result = feed.scoreItem(item);
      assert.equal(result.score, 0.6);
    });
    it("should add min_score of 0 if undefined", () => {
      const item = {};
      feed._prefCache.config = {
        personalized: true,
      };
      feed.affinityProvider = {
        calculateItemRelevanceScore: () => 0.5,
      };
      const result = feed.scoreItem(item);
      assert.equal(result.min_score, 0);
    });
  });

  describe("#_sendSpocsFill", () => {
    it("should send out all the SPOCS Fill pings", () => {
      sandbox.spy(feed.store, "dispatch");
      const expected = [
        { id: 1, reason: "frequency_cap", displayed: 0, full_recalc: 1 },
        { id: 2, reason: "frequency_cap", displayed: 0, full_recalc: 1 },
        { id: 3, reason: "blocked_by_user", displayed: 0, full_recalc: 1 },
        { id: 4, reason: "blocked_by_user", displayed: 0, full_recalc: 1 },
        { id: 5, reason: "campaign_duplicate", displayed: 0, full_recalc: 1 },
        { id: 6, reason: "campaign_duplicate", displayed: 0, full_recalc: 1 },
        { id: 7, reason: "below_min_score", displayed: 0, full_recalc: 1 },
        { id: 8, reason: "below_min_score", displayed: 0, full_recalc: 1 },
      ];
      const filtered = {
        frequency_cap: [{ id: 1, campaign_id: 1 }, { id: 2, campaign_id: 2 }],
        blocked_by_user: [{ id: 3, campaign_id: 3 }, { id: 4, campaign_id: 4 }],
        campaign_duplicate: [
          { id: 5, campaign_id: 5 },
          { id: 6, campaign_id: 6 },
        ],
        below_min_score: [{ id: 7, campaign_id: 7 }, { id: 8, campaign_id: 8 }],
      };
      feed._sendSpocsFill(filtered, true);

      assert.deepEqual(
        feed.store.dispatch.firstCall.args[0].data.spoc_fills,
        expected
      );
    });
    it("should send SPOCS Fill ping with the correct full_recalc", () => {
      sandbox.spy(feed.store, "dispatch");
      const expected = [
        { id: 1, reason: "frequency_cap", displayed: 0, full_recalc: 0 },
        { id: 2, reason: "frequency_cap", displayed: 0, full_recalc: 0 },
      ];
      const filtered = {
        frequency_cap: [{ id: 1, campaign_id: 1 }, { id: 2, campaign_id: 2 }],
      };
      feed._sendSpocsFill(filtered, false);

      assert.deepEqual(
        feed.store.dispatch.firstCall.args[0].data.spoc_fills,
        expected
      );
    });
    it("should not send non-SPOCS Fill pings", () => {
      sandbox.spy(feed.store, "dispatch");
      const expected = [
        { id: 1, reason: "frequency_cap", displayed: 0, full_recalc: 1 },
        { id: 3, reason: "blocked_by_user", displayed: 0, full_recalc: 1 },
        { id: 5, reason: "campaign_duplicate", displayed: 0, full_recalc: 1 },
        { id: 7, reason: "below_min_score", displayed: 0, full_recalc: 1 },
      ];
      const filtered = {
        frequency_cap: [{ id: 1, campaign_id: 1 }, { id: 2 }],
        blocked_by_user: [{ id: 3, campaign_id: 3 }, { id: 4 }],
        campaign_duplicate: [{ id: 5, campaign_id: 5 }, { id: 6 }],
        below_min_score: [{ id: 7, campaign_id: 7 }, { id: 8 }],
      };
      feed._sendSpocsFill(filtered, true);

      assert.deepEqual(
        feed.store.dispatch.firstCall.args[0].data.spoc_fills,
        expected
      );
    });
  });
});
