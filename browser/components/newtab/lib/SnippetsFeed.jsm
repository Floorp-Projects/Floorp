/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
const {actionTypes: at, actionCreators: ac} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});

ChromeUtils.defineModuleGetter(this, "AddonManager",
  "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(this, "ShellService",
  "resource:///modules/ShellService.jsm");
ChromeUtils.defineModuleGetter(this, "ProfileAge",
  "resource://gre/modules/ProfileAge.jsm");
ChromeUtils.defineModuleGetter(this, "FxAccounts",
  "resource://gre/modules/FxAccounts.jsm");
ChromeUtils.defineModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");

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

const ONE_DAY = 24 * 60 * 60 * 1000;
const ONE_WEEK = 7 * ONE_DAY;

this.SnippetsFeed = class SnippetsFeed {
  constructor() {
    this._refresh = this._refresh.bind(this);
    this._totalBookmarks = null;
    this._totalBookmarksLastUpdated = null;
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

  isDevtoolsUser() {
    return Services.prefs.getIntPref("devtools.selfxss.count") >= 5;
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

  async getAddonsInfo(target) {
    const {addons, fullData} = await AddonManager.getActiveAddons(["extension", "service"]);
    const info = {};
    for (const addon of addons) {
      info[addon.id] = {
        version: addon.version,
        type: addon.type,
        isSystem: addon.isSystem,
        isWebExtension: addon.isWebExtension
      };
      if (fullData) {
        Object.assign(info[addon.id], {
          name: addon.name,
          userDisabled: addon.userDisabled,
          installDate: addon.installDate
        });
      }
    }
    const data = {addons: info, isFullData: fullData};
    this.store.dispatch(ac.OnlyToOneContent({type: at.ADDONS_INFO_RESPONSE, data}, target));
  }

  async getTotalBookmarksCount(target) {
    if (!this._totalBookmarks || (Date.now() - this._totalBookmarksLastUpdated > ONE_DAY)) {
      this._totalBookmarksLastUpdated = Date.now();
      try {
        this._totalBookmarks = await NewTabUtils.activityStreamProvider.getTotalBookmarksCount();
      } catch (e) {
        Cu.reportError(e);
      }
    }
    this.store.dispatch(ac.OnlyToOneContent({type: at.TOTAL_BOOKMARKS_RESPONSE, data: this._totalBookmarks}, target));
  }

  _dispatchChanges(data) {
    this.store.dispatch(ac.BroadcastToContent({type: at.SNIPPETS_DATA, data}));
  }

  async _saveBlockedSnippet(snippetId) {
    const blockList = await this._getBlockList() || [];
    return this._storage.set("blockList", blockList.concat([snippetId]));
  }

  _getBlockList() {
    return this._storage.get("blockList");
  }

  _clearBlockList() {
    return this._storage.set("blockList", []);
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
      defaultBrowser: this.isDefaultBrowser(),
      isDevtoolsUser: this.isDevtoolsUser(),
      blockList: await this._getBlockList() || [],
      previousSessionEnd: this._previousSessionEnd
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
    this._storage = this.store.dbStorage.getDbTable("snippets");
    Services.obs.addObserver(this, SEARCH_ENGINE_OBSERVER_TOPIC);
    this._previousSessionEnd = await this._storage.get("previousSessionEnd");
    await this._refresh();
    Services.prefs.addObserver(ONBOARDING_FINISHED_PREF, this._refresh);
    Services.prefs.addObserver(SNIPPETS_URL_PREF, this._refresh);
    Services.prefs.addObserver(TELEMETRY_PREF, this._refresh);
    Services.prefs.addObserver(FXA_USERNAME_PREF, this._refresh);
  }

  uninit() {
    this._storage.set("previousSessionEnd", Date.now());
    Services.prefs.removeObserver(ONBOARDING_FINISHED_PREF, this._refresh);
    Services.prefs.removeObserver(SNIPPETS_URL_PREF, this._refresh);
    Services.prefs.removeObserver(TELEMETRY_PREF, this._refresh);
    Services.prefs.removeObserver(FXA_USERNAME_PREF, this._refresh);
    Services.obs.removeObserver(this, SEARCH_ENGINE_OBSERVER_TOPIC);
    this.store.dispatch(ac.BroadcastToContent({type: at.SNIPPETS_RESET}));
  }

  async showFirefoxAccounts(browser) {
    const url = await FxAccounts.config.promiseSignUpURI("snippets");
    // We want to replace the current tab.
    browser.loadURI(url);
  }

  async onAction(action) {
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
        this._saveBlockedSnippet(action.data);
        this.store.dispatch(ac.BroadcastToContent({type: at.SNIPPET_BLOCKED, data: action.data}));
        break;
      case at.SNIPPETS_BLOCKLIST_CLEARED:
        this._clearBlockList();
        break;
      case at.TOTAL_BOOKMARKS_REQUEST:
        this.getTotalBookmarksCount(action.meta.fromTarget);
        break;
      case at.ADDONS_INFO_REQUEST:
        await this.getAddonsInfo(action.meta.fromTarget);
        break;
    }
  }
};

const EXPORTED_SYMBOLS = ["SnippetsFeed"];
