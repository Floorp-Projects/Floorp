/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");

const PROVIDER_PREF = "browser.newtabpage.activity-stream.asrouter.messageProviders";
const DEVTOOLS_PREF = "browser.newtabpage.activity-stream.asrouter.devtoolsEnabled";

const DEFAULT_STATE = {
  _initialized: false,
  _providers: null,
  _providerPref: PROVIDER_PREF,
  _devtoolsEnabled: null,
  _devtoolsPref: DEVTOOLS_PREF,
};

const USER_PREFERENCES = {
  snippets: "browser.newtabpage.activity-stream.feeds.snippets",
  cfr: "browser.newtabpage.activity-stream.asrouter.userprefs.cfr",
};

const TEST_PROVIDER = {
  id: "snippets_local_testing",
  type: "local",
  localProvider: "SnippetsTestMessageProvider",
  enabled: true,
};

class _ASRouterPreferences {
  constructor() {
    Object.assign(this, DEFAULT_STATE);
    this._callbacks = new Set();
  }

  _getProviderConfig() {
    try {
      return JSON.parse(Services.prefs.getStringPref(this._providerPref, ""));
    } catch (e) {
      Cu.reportError(`Could not parse ASRouter preference. Try resetting ${this._providerPref} in about:config.`);
    }
    return null;
  }

  get providers() {
    if (!this._initialized || this._providers === null) {
      const config = this._getProviderConfig() || [];
      const providers = config.map(provider => Object.freeze(provider));
      if (this.devtoolsEnabled) {
        providers.unshift(TEST_PROVIDER);
      }
      this._providers = Object.freeze(providers);
    }

    return this._providers;
  }

  enableOrDisableProvider(id, value) {
    const providers = this._getProviderConfig();
    if (!providers) {
      Cu.reportError(`Cannot enable/disable providers if ${this._providerPref} is unparseable.`);
      return;
    }
    if (!providers.find(p => p.id === id)) {
      Cu.reportError(`Cannot set enabled state for '${id}' because it does not exist in ${this._providerPref}`);
      return;
    }

    const newConfig = providers.map(provider => {
      if (provider.id === id) {
        return {...provider, enabled: value};
      }
      return provider;
    });
    Services.prefs.setStringPref(this._providerPref, JSON.stringify(newConfig));
  }

  resetProviderPref() {
    Services.prefs.clearUserPref(this._providerPref);
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

  get specialConditions() {
    let allowLegacySnippets = true;
    for (const provider of this.providers) {
      if (provider.id === "snippets" && provider.enabled) {
        allowLegacySnippets = false;
      }
    }
    return {allowLegacySnippets};
  }

  observe(aSubject, aTopic, aPrefName) {
    switch (aPrefName) {
      case this._providerPref:
        this._providers = null;
        break;
      case this._devtoolsPref:
        this._providers = null;
        this._devtoolsEnabled = null;
        break;
    }
    this._callbacks.forEach(cb => cb(aPrefName));
  }

  getUserPreference(providerId) {
    if (!USER_PREFERENCES[providerId]) {
      return null;
    }
    return Services.prefs.getBoolPref(USER_PREFERENCES[providerId], true);
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
    Services.prefs.addObserver(this._providerPref, this);
    Services.prefs.addObserver(this._devtoolsPref, this);
    for (const id of Object.keys(USER_PREFERENCES)) {
      Services.prefs.addObserver(USER_PREFERENCES[id], this);
    }
    this._initialized = true;
  }

  uninit() {
    if (this._initialized) {
      Services.prefs.removeObserver(this._providerPref, this);
      Services.prefs.removeObserver(this._devtoolsPref, this);
      for (const id of Object.keys(USER_PREFERENCES)) {
        Services.prefs.removeObserver(USER_PREFERENCES[id], this);
      }
    }
    Object.assign(this, DEFAULT_STATE);
    this._callbacks.clear();
  }
}
this._ASRouterPreferences = _ASRouterPreferences;

this.ASRouterPreferences = new _ASRouterPreferences();
this.TEST_PROVIDER = TEST_PROVIDER;

const EXPORTED_SYMBOLS = ["_ASRouterPreferences", "ASRouterPreferences", "TEST_PROVIDER"];
