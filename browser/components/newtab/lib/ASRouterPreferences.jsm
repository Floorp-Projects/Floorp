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

class _ASRouterPreferences {
  constructor() {
    Object.assign(this, DEFAULT_STATE);
    this._callbacks = new Set();
  }

  get providers() {
    if (!this._initialized || this._providers === null) {
      try {
        const parsed = JSON.parse(Services.prefs.getStringPref(this._providerPref, ""));
        this._providers = Object.freeze(parsed.map(provider => Object.freeze(provider)));
      } catch (e) {
        Cu.reportError("Problem parsing JSON message provider pref for ASRouter");
        this._providers = [];
      }
    }
    return this._providers;
  }

  get devtoolsEnabled() {
    if (!this._initialized || this._devtoolsEnabled === null) {
      this._devtoolsEnabled = Services.prefs.getBoolPref(this._devtoolsPref, false);
    }
    return this._devtoolsEnabled;
  }

  get specialConditions() {
    let allowLegacyOnboarding = true;
    let allowLegacySnippets = true;
    for (const provider of this.providers) {
      if (provider.id === "onboarding" && provider.enabled && provider.cohort) {
        allowLegacyOnboarding = false;
      }
      if (provider.id === "snippets" && provider.enabled) {
        allowLegacySnippets = false;
      }
    }
    return {
      allowLegacyOnboarding,
      allowLegacySnippets,
    };
  }

  observe(aSubject, aTopic, aPrefName) {
    switch (aPrefName) {
      case this._providerPref:
        this._providers = null;
        break;
      case this._devtoolsPref:
        this._devtoolsEnabled = null;
        break;
    }
    this._callbacks.forEach(cb => cb(aPrefName));
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
    this._initialized = true;
  }

  uninit() {
    if (this._initialized) {
      Services.prefs.removeObserver(this._providerPref, this);
      Services.prefs.removeObserver(this._devtoolsPref, this);
    }
    Object.assign(this, DEFAULT_STATE);
    this._callbacks.clear();
  }
}
this._ASRouterPreferences = _ASRouterPreferences;

this.ASRouterPreferences = new _ASRouterPreferences();

const EXPORTED_SYMBOLS = ["_ASRouterPreferences", "ASRouterPreferences"];
