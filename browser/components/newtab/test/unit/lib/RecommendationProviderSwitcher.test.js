import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import { RecommendationProviderSwitcher } from "lib/RecommendationProviderSwitcher.jsm";
import { combineReducers, createStore } from "redux";
import { reducers } from "common/Reducers.jsm";
const PREF_PERSONALIZATION_VERSION = "discoverystream.personalization.version";
const PREF_PERSONALIZATION_MODEL_KEYS =
  "discoverystream.personalization.modelKeys";
describe("RecommendationProviderSwitcher", () => {
  let feed;
  let sandbox;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    feed = new RecommendationProviderSwitcher();
    feed.store = createStore(combineReducers(reducers), {});
  });

  afterEach(() => {
    sandbox.restore();
  });

  describe("#setAffinityProvider", () => {
    it("should setup proper affnity provider with modelKeys", async () => {
      feed.setAffinityProvider();

      assert.equal(feed.affinityProvider.modelKeys, undefined);

      feed.affinityProvider = null;
      feed.affinityProviderV2 = {
        modelKeys: "1234",
      };

      feed.setAffinityProvider();

      assert.equal(feed.affinityProvider.modelKeys, "1234");
    });
    it("should use old provider", async () => {
      feed.setAffinityProvider(
        undefined,
        undefined,
        undefined,
        undefined,
        undefined
      );

      assert.equal(feed.affinityProvider.modelKeys, undefined);

      feed.affinityProviderV2 = {
        modelKeys: "1234",
      };

      feed.setAffinityProvider(
        undefined,
        undefined,
        undefined,
        undefined,
        undefined
      );

      assert.equal(feed.affinityProvider.modelKeys, undefined);
    });
  });

  describe("#init", () => {
    it("should init affinityProvider then refreshContent", async () => {
      feed.affinityProvider = {
        init: sandbox.stub().resolves(),
      };
      await feed.init();
      assert.calledOnce(feed.affinityProvider.init);
    });
  });

  describe("#setVersion", () => {
    beforeEach(() => {
      sandbox.spy(feed.store, "dispatch");
    });
    it("should properly set affinity provider with version 1", async () => {
      feed.store.getState = () => ({
        Prefs: {
          values: {
            [PREF_PERSONALIZATION_MODEL_KEYS]: "1,2,3,4",
            [PREF_PERSONALIZATION_VERSION]: 1,
          },
        },
      });
      feed.setVersion();
      assert.calledWith(
        feed.store.dispatch,
        ac.BroadcastToContent({
          type: at.DISCOVERY_STREAM_PERSONALIZATION_VERSION,
          data: { version: 1 },
        })
      );
      assert.equal(feed.affinityProviderV2, null);
    });
    it("should properly set affinity provider with version 2", async () => {
      feed.store.getState = () => ({
        Prefs: {
          values: {
            [PREF_PERSONALIZATION_MODEL_KEYS]: "1,2,3,4",
            [PREF_PERSONALIZATION_VERSION]: 2,
          },
        },
      });
      feed.setVersion();
      assert.calledWith(
        feed.store.dispatch,
        ac.BroadcastToContent({
          type: at.DISCOVERY_STREAM_PERSONALIZATION_VERSION,
          data: { version: 2 },
        })
      );
      assert.deepEqual(feed.affinityProviderV2.modelKeys, ["1", "2", "3", "4"]);
    });
  });

  describe("#getAffinities", () => {
    it("should call affinityProvider.getAffinities", () => {
      feed.affinityProvider = {
        getAffinities: sandbox.stub().resolves(),
      };
      feed.getAffinities();
      assert.calledOnce(feed.affinityProvider.getAffinities);
    });
  });

  describe("#dispatchRelevanceScoreDuration", () => {
    it("should call dispatchRelevanceScoreDuration if available", () => {
      feed.affinityProvider = {
        dispatchRelevanceScoreDuration: sandbox.stub().returns(),
      };
      feed.dispatchRelevanceScoreDuration(0);
      assert.calledWith(
        feed.affinityProvider.dispatchRelevanceScoreDuration,
        0
      );
    });
    it("should fire PERSONALIZATION_V1_ITEM_RELEVANCE_SCORE_DURATION", () => {
      feed.affinityProvider = {};
      sandbox.spy(feed.store, "dispatch");
      feed.dispatchRelevanceScoreDuration(0);
      assert.calledWithMatch(
        feed.store.dispatch,
        ac.PerfEvent({
          event: "PERSONALIZATION_V1_ITEM_RELEVANCE_SCORE_DURATION",
        })
      );

      assert.calledOnce(feed.store.dispatch);
    });
  });

  describe("#calculateItemRelevanceScore", () => {
    it("should use personalized score with affinity provider", async () => {
      const item = {};
      feed.affinityProvider = {
        calculateItemRelevanceScore: async () => 0.5,
      };
      await feed.calculateItemRelevanceScore(item);
      assert.equal(item.score, 0.5);
    });
  });

  describe("#teardown", () => {
    it("should call affinityProvider.teardown ", () => {
      feed.affinityProvider = {
        teardown: sandbox.stub().resolves(),
      };
      feed.teardown();
      assert.calledOnce(feed.affinityProvider.teardown);
    });
  });

  describe("#resetState", () => {
    it("should null affinityProviderV2 and affinityProvider", () => {
      feed.affinityProviderV2 = {};
      feed.affinityProvider = {};

      feed.resetState();

      assert.equal(feed.affinityProviderV2, null);
      assert.equal(feed.affinityProvider, null);
    });
  });

  describe("#onAction: INIT", () => {
    it("should fire setVersion", async () => {
      sandbox.spy(feed, "setVersion");
      feed.onAction({
        type: at.INIT,
      });
      assert.calledOnce(feed.setVersion);
    });
  });

  describe("#onAction: DISCOVERY_STREAM_CONFIG_CHANGE", () => {
    it("should call teardown, resetState, and setVersion", async () => {
      sandbox.spy(feed, "teardown");
      sandbox.spy(feed, "resetState");
      sandbox.spy(feed, "setVersion");
      feed.onAction({
        type: at.DISCOVERY_STREAM_CONFIG_CHANGE,
      });
      assert.calledOnce(feed.teardown);
      assert.calledOnce(feed.resetState);
      assert.calledOnce(feed.setVersion);
    });
  });

  describe("#onAction: PREF_CHANGED", () => {
    beforeEach(() => {
      sandbox.spy(feed.store, "dispatch");
    });
    it("should dispatch to DISCOVERY_STREAM_CONFIG_RESET from PREF_PERSONALIZATION_VERSION", async () => {
      feed.onAction({
        type: at.PREF_CHANGED,
        data: {
          name: PREF_PERSONALIZATION_VERSION,
        },
      });

      assert.calledWith(
        feed.store.dispatch,
        ac.BroadcastToContent({
          type: at.DISCOVERY_STREAM_CONFIG_RESET,
        })
      );
    });
    it("should dispatch to DISCOVERY_STREAM_CONFIG_RESET PREF_PERSONALIZATION_MODEL_KEYS", async () => {
      feed.onAction({
        type: at.PREF_CHANGED,
        data: {
          name: PREF_PERSONALIZATION_MODEL_KEYS,
        },
      });

      assert.calledWith(
        feed.store.dispatch,
        ac.BroadcastToContent({
          type: at.DISCOVERY_STREAM_CONFIG_RESET,
        })
      );
    });
  });

  describe("#onAction: DISCOVERY_STREAM_PERSONALIZATION_VERSION_TOGGLE", () => {
    it("should fire SET_PREF with version", async () => {
      sandbox.spy(feed.store, "dispatch");
      feed.store.getState = () => ({
        Prefs: {
          values: {
            [PREF_PERSONALIZATION_VERSION]: 1,
          },
        },
      });

      await feed.onAction({
        type: at.DISCOVERY_STREAM_PERSONALIZATION_VERSION_TOGGLE,
      });
      assert.calledWith(
        feed.store.dispatch,
        ac.SetPref(PREF_PERSONALIZATION_VERSION, 2)
      );
    });
  });
});
