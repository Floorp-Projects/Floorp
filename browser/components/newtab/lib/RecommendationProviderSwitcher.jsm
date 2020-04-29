/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
ChromeUtils.defineModuleGetter(
  this,
  "UserDomainAffinityProvider",
  "resource://activity-stream/lib/UserDomainAffinityProvider.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PersonalityProvider",
  "resource://activity-stream/lib/PersonalityProvider/PersonalityProvider.jsm"
);
const { actionTypes: at, actionCreators: ac } = ChromeUtils.import(
  "resource://activity-stream/common/Actions.jsm"
);
const PREF_PERSONALIZATION_VERSION = "discoverystream.personalization.version";
const PREF_PERSONALIZATION_MODEL_KEYS =
  "discoverystream.personalization.modelKeys";

// The main purpose of this class is to handle interaction between one of the two providers.
// So all calls to the providers, anything involved with the switching between either provider,
// or anything involved with the setup or teardown of the switcher is contained in here.
this.RecommendationProviderSwitcher = class RecommendationProviderSwitcher {
  /**
   * setAffinityProvider - This function switches between setting up a v1 or v2
   *                     personalization provider.
   *                     It checks for certain configuration on affinityProviderV2,
   *                     which is setup in setAffinityProviderVersion.
   *                     In the case of v1, it returns a UserDomainAffinityProvider,
   *                     in the case of v2, it reutrns a PersonalityProvider.
   *
   *                     We need to do this swap because v2 is still being tested,
   *                     so by default v1 should be enabled.
   *                     This is why the function params are the same, as v2 has been
   *                     written to accept a similar pattern.
   *
   * @param {Array} timeSegments Changes the weight of a score based on how recent it is.
   * @param {Object} parameterSets Provides factors for weighting which allows for
   *                 flexible targeting. The functionality to calculate final scores can
   *                 be seen in UserDomainAffinityProvider#calculateScores
   * @param {Number} maxHistoryQueryResults How far back in the history do we go.
   * @param {Number} version What version of the provider does this use. Note, this is NOT
   *                 the same as personalization v1/v2, this could be used to change between
   *                 a configuration or value in the provider, not to enable/disable a whole
   *                 new provider.
   * @param {Object} scores This is used to re hydrate the provider based on cached results.
   * @returns {Object} A provider, either a PersonalityProvider or
   *                   UserDomainAffinityProvider.
   */
  setAffinityProvider(...args) {
    const { affinityProviderV2 } = this;
    if (affinityProviderV2 && affinityProviderV2.modelKeys) {
      // A v2 provider is already set. This can happen when new stories come in
      // and we need to update their scores.
      // We can use the existing one, a fresh one is created after startup.
      // Using the existing one might be a bit out of date,
      // but it's fine for now. We can rely on restarts for updates.
      // See bug 1629931 for improvements to this.
      if (this.affinityProvider) {
        return;
      }
      // At this point we've determined we can successfully create a v2 personalization provider.
      if (!this.affinityProvider) {
        this.affinityProvider = new PersonalityProvider({
          modelKeys: affinityProviderV2.modelKeys,
          dispatch: this.store.dispatch,
        });
      }
      this.affinityProvider.setAffinities(...args);
      return;
    }

    const start = Cu.now();
    // Otherwise, if we get this far, we fallback to a v1 personalization provider.
    this.affinityProvider = new UserDomainAffinityProvider(...args);
    this.store.dispatch(
      ac.PerfEvent({
        event: "topstories.domain.affinity.calculation.ms",
        value: Math.round(Cu.now() - start),
      })
    );
  }

  /*
   * This calls any async initialization that's required,
   * and then signals to devtools when that's done.
   * This is not the same as the switcher feed init,
   * this is to init the provider that the switcher controls.
   * The switcher feed init just calls setVersion.
   */
  async init() {
    if (this.affinityProvider && this.affinityProvider.init) {
      await this.affinityProvider.init();
      this.store.dispatch(
        ac.BroadcastToContent({
          type: at.DISCOVERY_STREAM_PERSONALIZATION_INIT,
        })
      );
    }
  }

  /**
   * Sets affinityProvider state to the correct version.
   */
  setVersion() {
    const version = this.store.getState().Prefs.values[
      PREF_PERSONALIZATION_VERSION
    ];
    const modelKeys = this.store.getState().Prefs.values[
      PREF_PERSONALIZATION_MODEL_KEYS
    ];
    if (version === 2 && modelKeys) {
      this.affinityProviderV2 = {
        modelKeys: modelKeys.split(",").map(i => i.trim()),
      };
    }

    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.DISCOVERY_STREAM_PERSONALIZATION_VERSION,
        data: {
          version,
        },
      })
    );
  }

  getAffinities() {
    return this.affinityProvider.getAffinities();
  }

  dispatchRelevanceScoreDuration(scoreStart) {
    if (this.affinityProvider) {
      if (this.affinityProvider.dispatchRelevanceScoreDuration) {
        this.affinityProvider.dispatchRelevanceScoreDuration(scoreStart);
      } else {
        this.store.dispatch(
          ac.PerfEvent({
            event: "PERSONALIZATION_V1_ITEM_RELEVANCE_SCORE_DURATION",
            value: Math.round(Cu.now() - scoreStart),
          })
        );
      }
    }
  }

  async calculateItemRelevanceScore(item) {
    if (this.affinityProvider) {
      const scoreResult = await this.affinityProvider.calculateItemRelevanceScore(
        item
      );
      if (scoreResult === 0 || scoreResult) {
        item.score = scoreResult;
      }
    }
  }

  teardown() {
    if (this.affinityProvider && this.affinityProvider.teardown) {
      // This removes any in memory listeners if available.
      this.affinityProvider.teardown();
    }
  }

  resetState() {
    this.affinityProviderV2 = null;
    this.affinityProvider = null;
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.setVersion();
        break;
      case at.DISCOVERY_STREAM_CONFIG_CHANGE:
        this.teardown();
        this.resetState();
        this.setVersion();
        break;
      case at.PREF_CHANGED:
        switch (action.data.name) {
          case PREF_PERSONALIZATION_VERSION:
          case PREF_PERSONALIZATION_MODEL_KEYS:
            this.store.dispatch(
              ac.BroadcastToContent({
                type: at.DISCOVERY_STREAM_CONFIG_RESET,
              })
            );
            break;
        }
        break;
      case at.DISCOVERY_STREAM_PERSONALIZATION_VERSION_TOGGLE:
        let version = this.store.getState().Prefs.values[
          PREF_PERSONALIZATION_VERSION
        ];

        // Toggle the version between 1 and 2.
        this.store.dispatch(
          ac.SetPref(PREF_PERSONALIZATION_VERSION, version === 1 ? 2 : 1)
        );
        break;
    }
  }
};

const EXPORTED_SYMBOLS = ["RecommendationProviderSwitcher"];
