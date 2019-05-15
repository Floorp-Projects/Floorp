/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

const PROVIDER_PREF_BRANCH = "browser.newtabpage.activity-stream.asrouter.providers.";
const DEVTOOLS_PREF = "browser.newtabpage.activity-stream.asrouter.devtoolsEnabled";
const FXA_USERNAME_PREF = "services.sync.username";

const DEFAULT_STATE = {
  _initialized: false,
  _providers: null,
  _providerPrefBranch: PROVIDER_PREF_BRANCH,
  _devtoolsEnabled: null,
  _devtoolsPref: DEVTOOLS_PREF,
};

const MIGRATE_PREFS = [
  // Old pref, New pref
  ["browser.newtabpage.activity-stream.asrouter.userprefs.cfr", "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.addons"],
];

const USER_PREFERENCES = {
  snippets: "browser.newtabpage.activity-stream.feeds.snippets",
  cfrAddons: "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.addons",
  cfrFeatures: "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
};

// Preferences that influence targeting attributes. When these change we need
// to re-evaluate if the message targeting still matches
const TARGETING_PREFERENCES = [FXA_USERNAME_PREF];

const TEST_PROVIDERS = [{
  id: "snippets_local_testing",
  type: "local",
  localProvider: "SnippetsTestMessageProvider",
  enabled: true,
}, {
  id: "panel_local_testing",
  type: "local",
  localProvider: "PanelTestProvider",
  enabled: true,
}];

class _ASRouterPreferences {
  constructor() {
    Object.assign(this, DEFAULT_STATE);
    this._callbacks = new Set();
  }

  _getProviderConfig() {
    const prefList = Services.prefs.getChildList(this._providerPrefBranch);
    return prefList.reduce((filtered, pref) => {
      let value;
      try {
        value = JSON.parse(Services.prefs.getStringPref(pref, ""));
      } catch (e) {
        Cu.reportError(`Could not parse ASRouter preference. Try resetting ${pref} in about:config.`);
      }
      if (value) {
        filtered.push(value);
      }
      return filtered;
    }, []);
  }

  // XXX Bug 1531734
  // Required for 67 when the pref change will happen
  _migratePrefs() {
    for (let [oldPref, newPref] of MIGRATE_PREFS) {
      if (!Services.prefs.prefHasUserValue(oldPref)) {
        continue;
      }
      if (Services.prefs.prefHasUserValue(newPref)) {
        Services.prefs.clearUserPref(oldPref);
        continue;
      }
      // If the pref was user modified we assume it was set to false
      const oldValue = Services.prefs.getBoolPref(oldPref, false);
      Services.prefs.clearUserPref(oldPref);
      Services.prefs.setBoolPref(newPref, oldValue);
    }
  }

  get providers() {
    if (!this._initialized || this._providers === null) {
      const config = this._getProviderConfig();
      const providers = config.map(provider => Object.freeze(provider));
      if (this.devtoolsEnabled) {
        providers.unshift(...TEST_PROVIDERS);
      }
      this._providers = Object.freeze(providers);
    }

    return this._providers;
  }

  enableOrDisableProvider(id, value) {
    const providers = this._getProviderConfig();
    const config = providers.find(p => p.id === id);
    if (!config) {
      Cu.reportError(`Cannot set enabled state for '${id}' because the pref ${this._providerPrefBranch}${id} does not exist or is not correctly formatted.`);
      return;
    }

    Services.prefs.setStringPref(this._providerPrefBranch + id, JSON.stringify({...config, enabled: value}));
  }

  resetProviderPref() {
    for (const pref of Services.prefs.getChildList(this._providerPrefBranch)) {
      Services.prefs.clearUserPref(pref);
    }
    for (const id of Object.keys(USER_PREFERENCES)) {
      Services.prefs.clearUserPref(USER_PREFERENCES[id]);
    }
  }

  get devtoolsEnabled() {
    if (!this._initialized || this._devtoolsEnabled === null) {
      this._devtoolsEnabled = Services.prefs.getBoolPref(this._devtoolsPref, false);
    }
    return this._devtoolsEnabled;
  }

  observe(aSubject, aTopic, aPrefName) {
    if (aPrefName && aPrefName.startsWith(this._providerPrefBranch)) {
      this._providers = null;
    } else if (aPrefName === this._devtoolsPref) {
      this._providers = null;
      this._devtoolsEnabled = null;
    }
    this._callbacks.forEach(cb => cb(aPrefName));
  }

  getUserPreference(providerId) {
    if (!USER_PREFERENCES[providerId]) {
      return null;
    }
    return Services.prefs.getBoolPref(USER_PREFERENCES[providerId], true);
  }

  getAllUserPreferences() {
    const values = {};
    for (const id of Object.keys(USER_PREFERENCES)) {
      values[id] = this.getUserPreference(id);
    }
    return values;
  }

  setUserPreference(providerId, value) {
    if (!USER_PREFERENCES[providerId]) {
      return;
    }
    Services.prefs.setBoolPref(USER_PREFERENCES[providerId], value);
  }

  addListener(callback) {
    this._callbacks.add(callback);
  }

  removeListener(callback) {
    this._callbacks.delete(callback);
  }

  init() {
    if (this._initialized) {
      return;
    }
    this._migratePrefs();
    Services.prefs.addObserver(this._providerPrefBranch, this);
    Services.prefs.addObserver(this._devtoolsPref, this);
    for (const id of Object.keys(USER_PREFERENCES)) {
      Services.prefs.addObserver(USER_PREFERENCES[id], this);
    }
    for (const targetingPref of TARGETING_PREFERENCES) {
      Services.prefs.addObserver(targetingPref, this);
    }
    this._initialized = true;
  }

  uninit() {
    if (this._initialized) {
      Services.prefs.removeObserver(this._providerPrefBranch, this);
      Services.prefs.removeObserver(this._devtoolsPref, this);
      for (const id of Object.keys(USER_PREFERENCES)) {
        Services.prefs.removeObserver(USER_PREFERENCES[id], this);
      }
      for (const targetingPref of TARGETING_PREFERENCES) {
        Services.prefs.removeObserver(targetingPref, this);
      }
    }
    Object.assign(this, DEFAULT_STATE);
    this._callbacks.clear();
  }
}
this._ASRouterPreferences = _ASRouterPreferences;

this.ASRouterPreferences = new _ASRouterPreferences();
this.TEST_PROVIDERS = TEST_PROVIDERS;
this.TARGETING_PREFERENCES = TARGETING_PREFERENCES;

const EXPORTED_SYMBOLS = ["_ASRouterPreferences", "ASRouterPreferences", "TEST_PROVIDERS", "TARGETING_PREFERENCES"];
