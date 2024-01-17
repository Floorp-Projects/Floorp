/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const PROVIDER_PREF_BRANCH =
  "browser.newtabpage.activity-stream.asrouter.providers.";
const DEVTOOLS_PREF =
  "browser.newtabpage.activity-stream.asrouter.devtoolsEnabled";

/**
 * Use `ASRouterPreferences.console.debug()` and friends from ASRouter files to
 * log messages during development.  See LOG_LEVELS in ConsoleAPI.jsm for the
 * available methods as well as the available values for this pref.
 */
const DEBUG_PREF = "browser.newtabpage.activity-stream.asrouter.debugLogLevel";

const FXA_USERNAME_PREF = "services.sync.username";

const DEFAULT_STATE = {
  _initialized: false,
  _providers: null,
  _providerPrefBranch: PROVIDER_PREF_BRANCH,
  _devtoolsEnabled: null,
  _devtoolsPref: DEVTOOLS_PREF,
};

const USER_PREFERENCES = {
  cfrAddons: "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.addons",
  cfrFeatures:
    "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
};

// Preferences that influence targeting attributes. When these change we need
// to re-evaluate if the message targeting still matches
const TARGETING_PREFERENCES = [FXA_USERNAME_PREF];

const TEST_PROVIDERS = [
  {
    id: "panel_local_testing",
    type: "local",
    localProvider: "PanelTestProvider",
    enabled: true,
  },
];

class _ASRouterPreferences {
  constructor() {
    Object.assign(this, DEFAULT_STATE);
    this._callbacks = new Set();

    ChromeUtils.defineLazyGetter(this, "console", () => {
      let { ConsoleAPI } = ChromeUtils.importESModule(
        "resource://gre/modules/Console.sys.mjs"
      );
      let consoleOptions = {
        maxLogLevel: "error",
        maxLogLevelPref: DEBUG_PREF,
        prefix: "ASRouter",
      };
      return new ConsoleAPI(consoleOptions);
    });
  }

  _transformPersonalizedCfrScores(value) {
    let result = {};
    try {
      result = JSON.parse(value);
    } catch (e) {
      console.error(e);
    }
    return result;
  }

  _getProviderConfig() {
    const prefList = Services.prefs.getChildList(this._providerPrefBranch);
    return prefList.reduce((filtered, pref) => {
      let value;
      try {
        value = JSON.parse(Services.prefs.getStringPref(pref, ""));
      } catch (e) {
        console.error(
          `Could not parse ASRouter preference. Try resetting ${pref} in about:config.`
        );
      }
      if (value) {
        filtered.push(value);
      }
      return filtered;
    }, []);
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
      console.error(
        `Cannot set enabled state for '${id}' because the pref ${this._providerPrefBranch}${id} does not exist or is not correctly formatted.`
      );
      return;
    }

    Services.prefs.setStringPref(
      this._providerPrefBranch + id,
      JSON.stringify({ ...config, enabled: value })
    );
  }

  resetProviderPref() {
    for (const pref of Services.prefs.getChildList(this._providerPrefBranch)) {
      Services.prefs.clearUserPref(pref);
    }
    for (const id of Object.keys(USER_PREFERENCES)) {
      Services.prefs.clearUserPref(USER_PREFERENCES[id]);
    }
  }

  /**
   * Bug 1800087 - Migrate the ASRouter message provider prefs' values to the
   * current format (provider.bucket -> provider.collection).
   *
   * TODO (Bug 1800937): Remove migration code after the next watershed release.
   */
  _migrateProviderPrefs() {
    const prefList = Services.prefs.getChildList(this._providerPrefBranch);
    for (const pref of prefList) {
      if (!Services.prefs.prefHasUserValue(pref)) {
        continue;
      }
      try {
        let value = JSON.parse(Services.prefs.getStringPref(pref, ""));
        if (value && "bucket" in value && !("collection" in value)) {
          const { bucket, ...rest } = value;
          Services.prefs.setStringPref(
            pref,
            JSON.stringify({
              ...rest,
              collection: bucket,
            })
          );
        }
      } catch (e) {
        Services.prefs.clearUserPref(pref);
      }
    }
  }

  get devtoolsEnabled() {
    if (!this._initialized || this._devtoolsEnabled === null) {
      this._devtoolsEnabled = Services.prefs.getBoolPref(
        this._devtoolsPref,
        false
      );
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

  getUserPreference(name) {
    const prefName = USER_PREFERENCES[name] || name;
    return Services.prefs.getBoolPref(prefName, true);
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
    this._migrateProviderPrefs();
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

const ASRouterPreferences = new _ASRouterPreferences();

const EXPORTED_SYMBOLS = [
  "_ASRouterPreferences",
  "ASRouterPreferences",
  "TEST_PROVIDERS",
  "TARGETING_PREFERENCES",
];
