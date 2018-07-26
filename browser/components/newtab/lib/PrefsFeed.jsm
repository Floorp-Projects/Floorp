/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {actionCreators: ac, actionTypes: at} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});
const {Prefs} = ChromeUtils.import("resource://activity-stream/lib/ActivityStreamPrefs.jsm", {});
const {PrerenderData} = ChromeUtils.import("resource://activity-stream/common/PrerenderData.jsm", {});
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");
const ONBOARDING_FINISHED_PREF = "browser.onboarding.notification.finished";

// List of prefs that require migration to indexedDB.
// Object key is the name of the pref in indexedDB, each will contain a
// map (key: name of preference to migrate, value: name of component).
const PREF_MIGRATION = {
  collapsed: new Map([
    ["collapseTopSites", "topsites"],
    ["section.highlights.collapsed", "highlights"],
    ["section.topstories.collapsed", "topstories"]
  ])
};

this.PrefsFeed = class PrefsFeed {
  constructor(prefMap) {
    this._prefMap = prefMap;
    this._prefs = new Prefs();
  }

  // If any of the prefs are set to something other than what the
  // prerendered version of AS expects, we can't use it.
  async _setPrerenderPref() {
    const indexedDBPrefs = await this._storage.getAll();
    const prefsAreValid = PrerenderData.arePrefsValid(pref => this._prefs.get(pref), indexedDBPrefs);
    this._prefs.set("prerender", prefsAreValid);
  }

  _checkPrerender(name) {
    if (PrerenderData.invalidatingPrefs.includes(name)) {
      this._setPrerenderPref();
    }
  }

  _initOnboardingPref() {
    const snippetsEnabled = this._prefs.get("feeds.snippets");
    if (!snippetsEnabled) {
      this.setOnboardingDisabledDefault(true);
    }
  }

  setOnboardingDisabledDefault(value) {
    const branch = Services.prefs.getDefaultBranch("");
    branch.setBoolPref(ONBOARDING_FINISHED_PREF, value);
  }

  onPrefChanged(name, value) {
    if (this._prefMap.has(name)) {
      this.store.dispatch(ac.BroadcastToContent({type: at.PREF_CHANGED, data: {name, value}}));
    }

    this._checkPrerender(name);

    if (name === "feeds.snippets") {
      // If snippets are disabled, onboarding notifications should also be
      // disabled because they look like snippets.
      this.setOnboardingDisabledDefault(!value);
    }
  }

  _migratePrefs() {
    for (const indexedDBPref of Object.keys(PREF_MIGRATION)) {
      for (const migratePref of PREF_MIGRATION[indexedDBPref].keys()) {
        // Check if pref exists (if the user changed the default)
        if (this._prefs.get(migratePref, null) === true) {
          const data = {id: PREF_MIGRATION[indexedDBPref].get(migratePref), value: {}};
          data.value[indexedDBPref] = true;
          this.store.dispatch(ac.OnlyToMain({type: at.UPDATE_SECTION_PREFS, data}));
          this._prefs.reset(migratePref);
        }
      }
    }
  }

  init() {
    this._prefs.observeBranch(this);
    this._storage = this.store.dbStorage.getDbTable("sectionPrefs");

    // Get the initial value of each activity stream pref
    const values = {};
    for (const name of this._prefMap.keys()) {
      values[name] = this._prefs.get(name);
    }

    // These are not prefs, but are needed to determine stuff in content that can only be
    // computed in main process
    values.isPrivateBrowsingEnabled = PrivateBrowsingUtils.enabled;
    values.platform = AppConstants.platform;

    // Get the firefox accounts url for links and to send firstrun metrics to.
    values.fxa_endpoint = Services.prefs.getStringPref(
      "browser.newtabpage.activity-stream.fxaccounts.endpoint", "https://accounts.firefox.com");

    // Set the initial state of all prefs in redux
    this.store.dispatch(ac.BroadcastToContent({type: at.PREFS_INITIAL_VALUES, data: values}));

    this._migratePrefs();
    this._setPrerenderPref();
    this._initOnboardingPref();
  }

  removeListeners() {
    this._prefs.ignoreBranch(this);
  }

  async _setIndexedDBPref(id, value) {
    const name = id === "topsites" ? id : `feeds.section.${id}`;
    try {
      await this._storage.set(name, value);
      this._setPrerenderPref();
    } catch (e) {
      Cu.reportError("Could not set section preferences.");
    }
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.UNINIT:
        this.removeListeners();
        this.setOnboardingDisabledDefault(false);
        break;
      case at.SET_PREF:
        this._prefs.set(action.data.name, action.data.value);
        break;
      case at.DISABLE_ONBOARDING:
        this.setOnboardingDisabledDefault(true);
        break;
      case at.UPDATE_SECTION_PREFS:
        this._setIndexedDBPref(action.data.id, action.data.value);
        break;
    }
  }
};

const EXPORTED_SYMBOLS = ["PrefsFeed"];
