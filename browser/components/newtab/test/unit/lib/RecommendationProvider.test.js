import {
  actionCreators as ac,
  actionTypes as at,
} from "common/Actions.sys.mjs";
import { RecommendationProvider } from "lib/RecommendationProvider.jsm";
import { combineReducers, createStore } from "redux";
import { reducers } from "common/Reducers.sys.mjs";
import { GlobalOverrider } from "test/unit/utils";

import { PersonalityProvider } from "lib/PersonalityProvider/PersonalityProvider.jsm";
import { PersistentCache } from "lib/PersistentCache.sys.mjs";

const PREF_PERSONALIZATION_ENABLED = "discoverystream.personalization.enabled";
const PREF_PERSONALIZATION_MODEL_KEYS =
  "discoverystream.personalization.modelKeys";
describe("RecommendationProvider", () => {
  let feed;
  let sandbox;
  let clock;
  let globals;

  beforeEach(() => {
    globals = new GlobalOverrider();
    globals.set({
      PersistentCache,
      PersonalityProvider,
    });

    sandbox = sinon.createSandbox();
    clock = sinon.useFakeTimers();
    feed = new RecommendationProvider();
    feed.store = createStore(combineReducers(reducers), {});
  });

  afterEach(() => {
    sandbox.restore();
    clock.restore();
    globals.restore();
  });

  describe("#setProvider", () => {
    it("should setup proper provider with modelKeys", async () => {
      feed.setProvider();

      assert.equal(feed.provider.modelKeys, undefined);

      feed.provider = null;
      feed._modelKeys = "1234";

      feed.setProvider();

      assert.equal(feed.provider.modelKeys, "1234");
      feed._modelKeys = "12345";

      // Calling it again should not rebuild the provider.
      feed.setProvider();
      assert.equal(feed.provider.modelKeys, "1234");
    });
  });

  describe("#calculateItemRelevanceScore", () => {
    it("should use personalized score with provider", async () => {
      const item = {};
      feed.provider = {
        calculateItemRelevanceScore: async () => 0.5,
      };
      await feed.calculateItemRelevanceScore(item);
      assert.equal(item.score, 0.5);
    });
  });

  describe("#teardown", () => {
    it("should call provider.teardown ", () => {
      sandbox.stub(global.Services.obs, "removeObserver").returns();
      feed.loaded = true;
      feed.provider = {
        teardown: sandbox.stub().resolves(),
      };
      feed.teardown();
      assert.calledOnce(feed.provider.teardown);
      assert.calledOnce(global.Services.obs.removeObserver);
      assert.calledWith(global.Services.obs.removeObserver, feed, "idle-daily");
    });
  });

  describe("#resetState", () => {
    it("should null affinityProviderV2 and affinityProvider", () => {
      feed._modelKeys = {};
      feed.provider = {};

      feed.resetState();

      assert.equal(feed._modelKeys, null);
      assert.equal(feed.provider, null);
    });
  });

  describe("#onAction: DISCOVERY_STREAM_CONFIG_CHANGE", () => {
    it("should call teardown, resetState, and setVersion", async () => {
      sandbox.spy(feed, "teardown");
      sandbox.spy(feed, "resetState");
      feed.onAction({
        type: at.DISCOVERY_STREAM_CONFIG_CHANGE,
      });
      assert.calledOnce(feed.teardown);
      assert.calledOnce(feed.resetState);
    });
  });

  describe("#onAction: PREF_CHANGED", () => {
    beforeEach(() => {
      sandbox.spy(feed.store, "dispatch");
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

  describe("#onAction: INIT", () => {
    it("should ", async () => {
      sandbox.stub(feed, "enable").returns();
      await feed.onAction({
        type: at.INIT,
      });
      assert.calledOnce(feed.enable);
      assert.calledWith(feed.enable, true);
    });
  });

  describe("#onAction: DISCOVERY_STREAM_PERSONALIZATION_OVERRIDE", () => {
    it("should ", async () => {
      sandbox.stub(feed, "personalizationOverride").returns();
      await feed.onAction({
        type: at.DISCOVERY_STREAM_PERSONALIZATION_OVERRIDE,
        data: { override: true },
      });
      assert.calledOnce(feed.personalizationOverride);
      assert.calledWith(feed.personalizationOverride, true);
    });
  });

  describe("#loadPersonalizationScoresCache", () => {
    it("should create a personalization provider from cached scores", async () => {
      sandbox.spy(feed.store, "dispatch");
      sandbox.spy(feed.cache, "set");
      feed.provider = {
        init: async () => {},
        getScores: () => "scores",
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

  describe("#updatePersonalizationScores", () => {
    beforeEach(() => {
      sandbox.spy(feed.store, "dispatch");
      sandbox.spy(feed.cache, "set");
      sandbox.spy(feed, "setProvider");
      feed.provider = {
        init: async () => {},
        getScores: () => "scores",
      };
    });
    it("should update provider on updatePersonalizationScores", async () => {
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

      await feed.updatePersonalizationScores();

      assert.calledWith(
        feed.store.dispatch,
        ac.BroadcastToContent({
          type: at.DISCOVERY_STREAM_PERSONALIZATION_LAST_UPDATED,
          data: {
            lastUpdated: 0,
          },
        })
      );
      assert.calledWith(feed.cache.set, "personalization", {
        scores: "scores",
        _timestamp: 0,
      });
    });
    it("should not update provider on updatePersonalizationScores", async () => {
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

      assert.notCalled(feed.setProvider);
    });
  });

  describe("#onAction: DISCOVERY_STREAM_PERSONALIZATION_TOGGLE", () => {
    it("should fire SET_PREF with enabled", async () => {
      sandbox.spy(feed.store, "dispatch");
      feed.store.getState = () => ({
        Prefs: {
          values: {
            [PREF_PERSONALIZATION_ENABLED]: false,
          },
        },
      });

      await feed.onAction({
        type: at.DISCOVERY_STREAM_PERSONALIZATION_TOGGLE,
      });
      assert.calledWith(
        feed.store.dispatch,
        ac.SetPref(PREF_PERSONALIZATION_ENABLED, true)
      );
    });
  });

  describe("#observe", () => {
    it("should call updatePersonalizationScores on idle daily", async () => {
      sandbox.stub(feed, "updatePersonalizationScores").returns();
      feed.observe(null, "idle-daily");
      assert.calledOnce(feed.updatePersonalizationScores);
    });
  });
});
