import {
  actionCreators as ac,
  actionTypes as at,
} from "common/Actions.sys.mjs";
import { RecommendationProvider } from "lib/RecommendationProvider.jsm";
import { combineReducers, createStore } from "redux";
import { reducers } from "common/Reducers.sys.mjs";
import { GlobalOverrider } from "test/unit/utils";

import { PersonalityProvider } from "lib/PersonalityProvider/PersonalityProvider.jsm";

const PREF_PERSONALIZATION_ENABLED = "discoverystream.personalization.enabled";
const PREF_PERSONALIZATION_MODEL_KEYS =
  "discoverystream.personalization.modelKeys";
describe("RecommendationProvider", () => {
  let feed;
  let sandbox;
  let globals;

  beforeEach(() => {
    globals = new GlobalOverrider();
    globals.set({
      PersonalityProvider,
    });

    sandbox = sinon.createSandbox();
    feed = new RecommendationProvider();
    feed.store = createStore(combineReducers(reducers), {});
  });

  afterEach(() => {
    sandbox.restore();
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

  describe("#init", () => {
    it("should init affinityProvider then refreshContent", async () => {
      feed.provider = {
        init: sandbox.stub().resolves(),
      };
      await feed.init();
      assert.calledOnce(feed.provider.init);
    });
  });

  describe("#getScores", () => {
    it("should call affinityProvider.getScores", () => {
      feed.provider = {
        getScores: sandbox.stub().resolves(),
      };
      feed.getScores();
      assert.calledOnce(feed.provider.getScores);
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
      feed.provider = {
        teardown: sandbox.stub().resolves(),
      };
      feed.teardown();
      assert.calledOnce(feed.provider.teardown);
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
});
