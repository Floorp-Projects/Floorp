/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
const {actionTypes: at, actionCreators: ac} = Cu.import("resource://activity-stream/common/Actions.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "ShellService",
  "resource:///modules/ShellService.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ProfileAge",
  "resource://gre/modules/ProfileAge.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm");

// Url to fetch snippets, in the urlFormatter service format.
const SNIPPETS_URL_PREF = "browser.aboutHomeSnippets.updateUrl";
const TELEMETRY_PREF = "datareporting.healthreport.uploadEnabled";
const FXA_USERNAME_PREF = "services.sync.username";
const ONBOARDING_FINISHED_PREF = "browser.onboarding.notification.finished";
// Prefix for any target matching a search engine.
const TARGET_SEARCHENGINE_PREFIX = "searchEngine-";

const SEARCH_ENGINE_OBSERVER_TOPIC = "browser-search-engine-modified";

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

  isDefaultBrowser() {
    try {
      return ShellService.isDefaultBrowser();
    } catch (e) {}
    // istanbul ignore next
    return null;
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

  getSelectedSearchEngine() {
    return new Promise(resolve => {
      // Note: calling init ensures this code is only executed after Search has been initialized
      Services.search.init(rv => {
        // istanbul ignore else
        if (Components.isSuccessCode(rv)) {
          let engines = Services.search.getVisibleEngines();
          resolve({
            searchEngineIdentifier: Services.search.defaultEngine.identifier,
            engines: engines
              .filter(engine => engine.identifier)
              .map(engine => `${TARGET_SEARCHENGINE_PREFIX}${engine.identifier}`)
          });
        } else {
          resolve({engines: [], searchEngineIdentifier: ""});
        }
      });
    });
  }

  _dispatchChanges(data) {
    this.store.dispatch(ac.BroadcastToContent({type: at.SNIPPETS_DATA, data}));
  }

  async _refresh() {
    const profileInfo = await this.getProfileInfo();
    const data = {
      profileCreatedWeeksAgo: profileInfo.createdWeeksAgo,
      profileResetWeeksAgo: profileInfo.resetWeeksAgo,
      snippetsURL: this.snippetsURL,
      version: STARTPAGE_VERSION,
      telemetryEnabled: Services.prefs.getBoolPref(TELEMETRY_PREF),
      onboardingFinished: Services.prefs.getBoolPref(ONBOARDING_FINISHED_PREF),
      fxaccount: Services.prefs.prefHasUserValue(FXA_USERNAME_PREF),
      selectedSearchEngine: await this.getSelectedSearchEngine(),
      defaultBrowser: this.isDefaultBrowser()
    };
    this._dispatchChanges(data);
  }

  async observe(subject, topic, data) {
    if (topic === SEARCH_ENGINE_OBSERVER_TOPIC) {
      const selectedSearchEngine = await this.getSelectedSearchEngine();
      this._dispatchChanges({selectedSearchEngine});
    }
  }

  async init() {
    await this._refresh();
    Services.prefs.addObserver(ONBOARDING_FINISHED_PREF, this._refresh);
    Services.prefs.addObserver(SNIPPETS_URL_PREF, this._refresh);
    Services.prefs.addObserver(TELEMETRY_PREF, this._refresh);
    Services.prefs.addObserver(FXA_USERNAME_PREF, this._refresh);
    Services.obs.addObserver(this, SEARCH_ENGINE_OBSERVER_TOPIC);
  }

  uninit() {
    Services.prefs.removeObserver(ONBOARDING_FINISHED_PREF, this._refresh);
    Services.prefs.removeObserver(SNIPPETS_URL_PREF, this._refresh);
    Services.prefs.removeObserver(TELEMETRY_PREF, this._refresh);
    Services.prefs.removeObserver(FXA_USERNAME_PREF, this._refresh);
    Services.obs.removeObserver(this, SEARCH_ENGINE_OBSERVER_TOPIC);
    this.store.dispatch(ac.BroadcastToContent({type: at.SNIPPETS_RESET}));
  }

  async showFirefoxAccounts(browser) {
    const url = await fxAccounts.promiseAccountsSignUpURI("snippets");
    // We want to replace the current tab.
    browser.loadURI(url);
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.UNINIT:
        this.uninit();
        break;
      case at.SHOW_FIREFOX_ACCOUNTS:
        this.showFirefoxAccounts(action._target.browser);
        break;
      case at.SNIPPETS_BLOCKLIST_UPDATED:
        this.store.dispatch(ac.BroadcastToContent({type: at.SNIPPET_BLOCKED, data: action.data}));
        break;
    }
  }
};

this.EXPORTED_SYMBOLS = ["SnippetsFeed"];
