/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
const {actionTypes: at, actionCreators: ac} = Cu.import("resource://activity-stream/common/Actions.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "ProfileAge",
  "resource://gre/modules/ProfileAge.jsm");

// Url to fetch snippets, in the urlFormatter service format.
const SNIPPETS_URL_PREF = "browser.aboutHomeSnippets.updateUrl";
const TELEMETRY_PREF = "datareporting.healthreport.uploadEnabled";
const ONBOARDING_FINISHED_PREF = "browser.onboarding.notification.finished";

// Should be bumped up if the snippets content format changes.
const STARTPAGE_VERSION = 5;

const ONE_WEEK = 7 * 24 * 60 * 60 * 1000;

this.SnippetsFeed = class SnippetsFeed {
  constructor() {
    this._refresh = this._refresh.bind(this);
  }
  get snippetsURL() {
    const updateURL = Services
      .prefs.getStringPref(SNIPPETS_URL_PREF)
      .replace("%STARTPAGE_VERSION%", STARTPAGE_VERSION);
    return Services.urlFormatter.formatURL(updateURL);
  }
  async getProfileInfo() {
    const profileAge = new ProfileAge(null, null);
    const createdDate = await profileAge.created;
    const resetDate = await profileAge.reset;
    return {
      createdWeeksAgo:  Math.floor((Date.now() - createdDate) / ONE_WEEK),
      resetWeeksAgo: resetDate ? Math.floor((Date.now() - resetDate) / ONE_WEEK) : null
    };
  }
  async _refresh() {
    const profileInfo = await this.getProfileInfo();
    const data = {
      snippetsURL: this.snippetsURL,
      version: STARTPAGE_VERSION,
      profileCreatedWeeksAgo: profileInfo.createdWeeksAgo,
      profileResetWeeksAgo: profileInfo.resetWeeksAgo,
      telemetryEnabled: Services.prefs.getBoolPref(TELEMETRY_PREF),
      onboardingFinished: Services.prefs.getBoolPref(ONBOARDING_FINISHED_PREF)
    };

    this.store.dispatch(ac.BroadcastToContent({type: at.SNIPPETS_DATA, data}));
  }
  async init() {
    await this._refresh();
    Services.prefs.addObserver(ONBOARDING_FINISHED_PREF, this._refresh);
    Services.prefs.addObserver(SNIPPETS_URL_PREF, this._refresh);
    Services.prefs.addObserver(TELEMETRY_PREF, this._refresh);
  }
  uninit() {
    Services.prefs.removeObserver(ONBOARDING_FINISHED_PREF, this._refresh);
    Services.prefs.removeObserver(SNIPPETS_URL_PREF, this._refresh);
    Services.prefs.removeObserver(TELEMETRY_PREF, this._refresh);
    this.store.dispatch({type: at.SNIPPETS_RESET});
  }
  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.FEED_INIT:
        if (action.data === "feeds.snippets") { this.init(); }
        break;
    }
  }
};

this.EXPORTED_SYMBOLS = ["SnippetsFeed"];
