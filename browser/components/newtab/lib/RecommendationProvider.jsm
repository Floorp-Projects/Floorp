/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Use XPCOMUtils.defineLazyModuleGetters to make the test harness keeps working
// after bug 1608279.
//
// The test harness's workaround for "lazy getter on a plain object" is to
// set the `lazy` object's prototype to the global object, inside the lazy
// getter API.
//
// ChromeUtils.defineModuleGetter is converted into a static import declaration
// by babel-plugin-jsm-to-esmodules, and it doesn't work for the following
// 2 reasons:
//
//   * There's no other lazy getter API call in this file, and the workaround
//     above stops working
//   * babel-plugin-jsm-to-esmodules ignores the first parameter of the lazy
//     getter API, and the result is wrong
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  PersistentCache: "resource://activity-stream/lib/PersistentCache.sys.mjs",
});
XPCOMUtils.defineLazyModuleGetters(lazy, {
  PersonalityProvider:
    "resource://activity-stream/lib/PersonalityProvider/PersonalityProvider.jsm",
});

const { actionTypes: at, actionCreators: ac } = ChromeUtils.importESModule(
  "resource://activity-stream/common/Actions.sys.mjs"
);
const CACHE_KEY = "personalization";
const PREF_PERSONALIZATION_MODEL_KEYS =
  "discoverystream.personalization.modelKeys";
const PREF_USER_TOPSTORIES = "feeds.section.topstories";
const PREF_SYSTEM_TOPSTORIES = "feeds.system.topstories";
const PREF_PERSONALIZATION = "discoverystream.personalization.enabled";
const MIN_PERSONALIZATION_UPDATE_TIME = 12 * 60 * 60 * 1000; // 12 hours
const PREF_PERSONALIZATION_OVERRIDE =
  "discoverystream.personalization.override";

// The main purpose of this class is to handle interactions with the recommendation provider.
// A recommendation provider scores a list of stories, currently this is a personality provider.
// So all calls to the provider, anything involved with the setup of the provider,
// accessing prefs for the provider, or updaing devtools with provider state, is contained in here.
class RecommendationProvider {
  constructor() {
    // Persistent cache for remote endpoint data.
    this.cache = new lazy.PersistentCache(CACHE_KEY, true);
  }

  async setProvider(isStartup = false, scores) {
    // A provider is already set. This can happen when new stories come in
    // and we need to update their scores.
    // We can use the existing one, a fresh one is created after startup.
    // Using the existing one might be a bit out of date,
    // but it's fine for now. We can rely on restarts for updates.
    // See bug 1629931 for improvements to this.
    if (!this.provider) {
      this.provider = new lazy.PersonalityProvider(this.modelKeys);
      this.provider.setScores(scores);
    }

    if (this.provider && this.provider.init) {
      await this.provider.init();
      this.store.dispatch(
        ac.BroadcastToContent({
          type: at.DISCOVERY_STREAM_PERSONALIZATION_INIT,
          meta: {
            isStartup,
          },
        })
      );
    }
  }

  async enable(isStartup) {
    await this.loadPersonalizationScoresCache(isStartup);
    Services.obs.addObserver(this, "idle-daily");
    this.loaded = true;
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.DISCOVERY_STREAM_PERSONALIZATION_UPDATED,
        meta: {
          isStartup,
        },
      })
    );
  }

  get showStories() {
    // Combine user-set stories opt-out with Mozilla-set config
    return (
      this.store.getState().Prefs.values[PREF_SYSTEM_TOPSTORIES] &&
      this.store.getState().Prefs.values[PREF_USER_TOPSTORIES]
    );
  }

  get personalized() {
    // If stories are not displayed, no point in trying to personalize them.
    if (!this.showStories) {
      return false;
    }
    const spocsPersonalized =
      this.store.getState().Prefs.values?.pocketConfig?.spocsPersonalized;
    const recsPersonalized =
      this.store.getState().Prefs.values?.pocketConfig?.recsPersonalized;
    const personalization =
      this.store.getState().Prefs.values[PREF_PERSONALIZATION];

    // There is a server sent flag to keep personalization on.
    // If the server stops sending this, we turn personalization off,
    // until the server starts returning the signal.
    const overrideState =
      this.store.getState().Prefs.values[PREF_PERSONALIZATION_OVERRIDE];

    return (
      personalization &&
      !overrideState &&
      (spocsPersonalized || recsPersonalized)
    );
  }

  get modelKeys() {
    if (!this._modelKeys) {
      this._modelKeys =
        this.store.getState().Prefs.values[PREF_PERSONALIZATION_MODEL_KEYS];
    }

    return this._modelKeys;
  }

  /*
   * This creates a new recommendationProvider using fresh data,
   * It's run on a last updated timer. This is the opposite of loadPersonalizationScoresCache.
   * This is also much slower so we only trigger this in the background on idle-daily.
   * It causes new profiles to pick up personalization slowly because the first time
   * a new profile is run you don't have any old cache to use, so it needs to wait for the first
   * idle-daily. Older profiles can rely on cache during the idle-daily gap. Idle-daily is
   * usually run once every 24 hours.
   */
  async updatePersonalizationScores() {
    if (
      !this.personalized ||
      Date.now() - this.personalizationLastUpdated <
        MIN_PERSONALIZATION_UPDATE_TIME
    ) {
      return;
    }

    await this.setProvider();

    const personalization = { scores: this.provider.getScores() };
    this.personalizationLastUpdated = Date.now();

    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.DISCOVERY_STREAM_PERSONALIZATION_LAST_UPDATED,
        data: {
          lastUpdated: this.personalizationLastUpdated,
        },
      })
    );
    personalization._timestamp = this.personalizationLastUpdated;
    this.cache.set("personalization", personalization);
  }

  /*
   * This just re hydrates the provider from cache.
   * We can call this on startup because it's generally fast.
   * It reports to devtools the last time the data in the cache was updated.
   */
  async loadPersonalizationScoresCache(isStartup = false) {
    const cachedData = (await this.cache.get()) || {};
    const { personalization } = cachedData;

    if (this.personalized && personalization?.scores) {
      await this.setProvider(isStartup, personalization.scores);

      this.personalizationLastUpdated = personalization._timestamp;

      this.store.dispatch(
        ac.BroadcastToContent({
          type: at.DISCOVERY_STREAM_PERSONALIZATION_LAST_UPDATED,
          data: {
            lastUpdated: this.personalizationLastUpdated,
          },
          meta: {
            isStartup,
          },
        })
      );
    }
  }

  // This turns personalization on/off if the server sends the override command.
  // The server sends a true signal to keep personalization on. So a malfunctioning
  // server would more likely mistakenly turn off personalization, and not turn it on.
  // This is safer, because the override is for cases where personalization is causing issues.
  // So having it mistakenly go off is safe, but it mistakenly going on could be bad.
  personalizationOverride(overrideCommand) {
    // Are we currently in an override state.
    // This is useful to know if we want to do a cleanup.
    const overrideState =
      this.store.getState().Prefs.values[PREF_PERSONALIZATION_OVERRIDE];

    // Is this profile currently set to be personalized.
    const personalization =
      this.store.getState().Prefs.values[PREF_PERSONALIZATION];

    // If we have an override command, profile is currently personalized,
    // and is not currently being overridden, we can set the override pref.
    if (overrideCommand && personalization && !overrideState) {
      this.store.dispatch(ac.SetPref(PREF_PERSONALIZATION_OVERRIDE, true));
    }

    // This is if we need to revert an override and do cleanup.
    // We do this if we are in an override state,
    // but not currently receiving the override signal.
    if (!overrideCommand && overrideState) {
      this.store.dispatch({
        type: at.CLEAR_PREF,
        data: { name: PREF_PERSONALIZATION_OVERRIDE },
      });
    }
  }

  async calculateItemRelevanceScore(item) {
    if (this.provider) {
      const scoreResult = await this.provider.calculateItemRelevanceScore(item);
      if (scoreResult === 0 || scoreResult) {
        item.score = scoreResult;
      }
    }
  }

  teardown() {
    if (this.provider && this.provider.teardown) {
      // This removes any in memory listeners if available.
      this.provider.teardown();
    }
    if (this.loaded) {
      Services.obs.removeObserver(this, "idle-daily");
    }
    this.loaded = false;
  }

  async resetState() {
    this._modelKeys = null;
    this.personalizationLastUpdated = null;
    this.provider = null;
    await this.cache.set("personalization", {});
    this.store.dispatch(
      ac.OnlyToMain({
        type: at.DISCOVERY_STREAM_PERSONALIZATION_RESET,
      })
    );
  }

  async observe(subject, topic, data) {
    switch (topic) {
      case "idle-daily":
        await this.updatePersonalizationScores();
        this.store.dispatch(
          ac.BroadcastToContent({
            type: at.DISCOVERY_STREAM_PERSONALIZATION_UPDATED,
          })
        );
        break;
    }
  }

  async onAction(action) {
    switch (action.type) {
      case at.INIT:
        await this.enable(true /* isStartup */);
        break;
      case at.DISCOVERY_STREAM_CONFIG_CHANGE:
        this.teardown();
        await this.resetState();
        await this.enable();
        break;
      case at.DISCOVERY_STREAM_DEV_IDLE_DAILY:
        Services.obs.notifyObservers(null, "idle-daily");
        break;
      case at.PREF_CHANGED:
        switch (action.data.name) {
          case PREF_PERSONALIZATION_MODEL_KEYS:
            this.store.dispatch(
              ac.BroadcastToContent({
                type: at.DISCOVERY_STREAM_CONFIG_RESET,
              })
            );
            break;
        }
        break;
      case at.DISCOVERY_STREAM_PERSONALIZATION_TOGGLE:
        let enabled = this.store.getState().Prefs.values[PREF_PERSONALIZATION];
        this.store.dispatch(ac.SetPref(PREF_PERSONALIZATION, !enabled));
        break;
      case at.DISCOVERY_STREAM_PERSONALIZATION_OVERRIDE:
        this.personalizationOverride(action.data.override);
        break;
    }
  }
}

const EXPORTED_SYMBOLS = ["RecommendationProvider"];
