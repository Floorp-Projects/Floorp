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
XPCOMUtils.defineLazyModuleGetters(lazy, {
  PersonalityProvider:
    "resource://activity-stream/lib/PersonalityProvider/PersonalityProvider.jsm",
});

const { actionTypes: at, actionCreators: ac } = ChromeUtils.import(
  "resource://activity-stream/common/Actions.jsm"
);
const PREF_PERSONALIZATION_MODEL_KEYS =
  "discoverystream.personalization.modelKeys";
const PREF_PERSONALIZATION = "discoverystream.personalization.enabled";

// The main purpose of this class is to handle interactions with the recommendation provider.
// A recommendation provider scores a list of stories, currently this is a personality provider.
// So all calls to the provider, anything involved with the setup of the provider,
// accessing prefs for the provider, or updaing devtools with provider state, is contained in here.
class RecommendationProvider {
  setProvider(scores) {
    // A provider is already set. This can happen when new stories come in
    // and we need to update their scores.
    // We can use the existing one, a fresh one is created after startup.
    // Using the existing one might be a bit out of date,
    // but it's fine for now. We can rely on restarts for updates.
    // See bug 1629931 for improvements to this.
    if (this.provider) {
      return;
    }
    // At this point we've determined we can successfully create a v2 personalization provider.
    this.provider = new lazy.PersonalityProvider(this.modelKeys);
    this.provider.setScores(scores);
  }

  /*
   * This calls any async initialization that's required,
   * and then signals to devtools when that's done.
   */
  async init() {
    if (this.provider && this.provider.init) {
      await this.provider.init();
      this.store.dispatch(
        ac.BroadcastToContent({
          type: at.DISCOVERY_STREAM_PERSONALIZATION_INIT,
        })
      );
    }
  }

  get modelKeys() {
    if (!this._modelKeys) {
      this._modelKeys = this.store.getState().Prefs.values[
        PREF_PERSONALIZATION_MODEL_KEYS
      ];
    }

    return this._modelKeys;
  }

  getScores() {
    return this.provider.getScores();
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
  }

  resetState() {
    this._modelKeys = null;
    this.provider = null;
  }

  onAction(action) {
    switch (action.type) {
      case at.DISCOVERY_STREAM_CONFIG_CHANGE:
        this.teardown();
        this.resetState();
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
    }
  }
}

const EXPORTED_SYMBOLS = ["RecommendationProvider"];
