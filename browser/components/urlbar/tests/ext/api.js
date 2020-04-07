/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global ExtensionAPI */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
});

XPCOMUtils.defineLazyGetter(
  this,
  "defaultPreferences",
  () => new Preferences({ defaultBranch: true })
);

this.experiments_urlbar = class extends ExtensionAPI {
  getAPI() {
    return {
      experiments: {
        urlbar: {
          clearInput() {
            let window = BrowserWindowTracker.getTopWindow();
            window.gURLBar.value = "";
            window.gURLBar.setPageProxyState("invalid");
          },

          engagementTelemetry: this._getDefaultSettingsAPI(
            "browser.urlbar.eventTelemetry.enabled"
          ),

          openViewOnFocus: this._getDefaultSettingsAPI(
            "browser.urlbar.openViewOnFocus"
          ),
        },
      },
    };
  }

  onShutdown() {
    // Reset the default prefs.  This is necessary because
    // ExtensionPreferencesManager doesn't properly reset prefs set on the
    // default branch.  See bug 1586543, bug 1578513, bug 1578508.
    if (this._initialDefaultPrefs) {
      for (let [pref, value] of this._initialDefaultPrefs.entries()) {
        defaultPreferences.set(pref, value);
      }
    }
  }

  _getDefaultSettingsAPI(pref) {
    return {
      get: details => {
        return {
          value: Preferences.get(pref),

          // Nothing actually uses this, but on debug builds there are extra
          // checks enabled in Schema.jsm that fail if it's not present.  The
          // value doesn't matter.
          levelOfControl: "controllable_by_this_extension",
        };
      },
      set: details => {
        if (!this._initialDefaultPrefs) {
          this._initialDefaultPrefs = new Map();
        }
        if (!this._initialDefaultPrefs.has(pref)) {
          this._initialDefaultPrefs.set(pref, defaultPreferences.get(pref));
        }
        defaultPreferences.set(pref, details.value);
        return true;
      },
      clear: details => {
        if (this._initialDefaultPrefs && this._initialDefaultPrefs.has(pref)) {
          defaultPreferences.set(pref, this._initialDefaultPrefs.get(pref));
          return true;
        }
        return false;
      },
    };
  }
};
