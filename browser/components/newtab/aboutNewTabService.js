/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/* globals XPCOMUtils, NewTabPrefsProvider, Services,
  Locale, UpdateUtils
*/
"use strict";

const {utils: Cu, interfaces: Ci} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
                                  "resource://gre/modules/UpdateUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabPrefsProvider",
                                  "resource:///modules/NewTabPrefsProvider.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Locale",
                                  "resource://gre/modules/Locale.jsm");

const LOCAL_NEWTAB_URL = "chrome://browser/content/newtab/newTab.xhtml";

const REMOTE_NEWTAB_URL = "https://newtab.cdn.mozilla.net/" +
                              "v%VERSION%/%CHANNEL%/%LOCALE%/index.html";

// Pref that tells if remote newtab is enabled
const PREF_REMOTE_ENABLED = "browser.newtabpage.remote";

// The preference that tells whether to match the OS locale
const PREF_MATCH_OS_LOCALE = "intl.locale.matchOS";

// The preference that tells what locale the user selected
const PREF_SELECTED_LOCALE = "general.useragent.locale";

const VALID_CHANNELS = new Set(["esr", "release", "beta", "aurora", "nightly"]);

const REMOTE_NEWTAB_VERSION = "0";

function AboutNewTabService() {
  NewTabPrefsProvider.prefs.on(PREF_REMOTE_ENABLED, this._handleToggleEvent.bind(this));

  // trigger remote change if needed, according to pref
  this.toggleRemote(Services.prefs.getBoolPref(PREF_REMOTE_ENABLED));
}

AboutNewTabService.prototype = {

  _newTabURL: LOCAL_NEWTAB_URL,
  _remoteEnabled: false,
  _overridden: false,

  classID: Components.ID("{cef25b06-0ef6-4c50-a243-e69f943ef23d}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAboutNewTabService]),
  _xpcom_categories: [{
    service: true
  }],

  _handleToggleEvent(prefName, stateEnabled, forceState) { //jshint unused:false
    this.toggleRemote(stateEnabled, forceState);
  },

  /**
   * React to changes to the remote newtab pref. Only act
   * if there is a change of state and if not overridden.
   *
   * @returns {Boolean} Returns if there has been a state change
   *
   * @param {Boolean}   stateEnabled    remote state to set to
   * @param {Boolean}   forceState      force state change
   */
  toggleRemote(stateEnabled, forceState) {

    if (!forceState && (this._overriden || stateEnabled === this._remoteEnabled)) {
      // exit there is no change of state
      return false;
    }

    if (stateEnabled) {
      this._newTabURL = this.generateRemoteURL();
      NewTabPrefsProvider.prefs.on(
        PREF_SELECTED_LOCALE,
        this._updateRemoteMaybe.bind(this));
      NewTabPrefsProvider.prefs.on(
        PREF_MATCH_OS_LOCALE,
        this._updateRemoteMaybe.bind(this));
      this._remoteEnabled = true;
    } else {
      this._newTabURL = LOCAL_NEWTAB_URL;
      NewTabPrefsProvider.prefs.off(PREF_SELECTED_LOCALE, this._updateRemoteMaybe);
      NewTabPrefsProvider.prefs.off(PREF_MATCH_OS_LOCALE, this._updateRemoteMaybe);
      this._remoteEnabled = false;
    }
    return true;
  },

  /*
   * Generate a default url based on locale and update channel
   */
  generateRemoteURL() {
    let releaseName = this.releaseFromUpdateChannel(UpdateUtils.UpdateChannel);
    let url = REMOTE_NEWTAB_URL
      .replace("%VERSION%", REMOTE_NEWTAB_VERSION)
      .replace("%LOCALE%", Locale.getLocale())
      .replace("%CHANNEL%", releaseName);
    return url;
  },

  /*
   * Updates the remote location when the page is not overriden.
   *
   * Useful when there is a dependent pref change
   */
  _updateRemoteMaybe() {
    if (!this._remoteEnabled || this._overridden) {
      return;
    }

    let url = this.generateRemoteURL();
    if (url !== this._newTabURL) {
      this._newTabURL = url;
      Services.obs.notifyObservers(null, "newtab-url-changed",
        this._newTabURL);
    }
  },

  /**
   * Returns the release name from an Update Channel name
   *
   * @return {String} a release name based on the update channel. Defaults to nightly
   */
  releaseFromUpdateChannel(channelName) {
    return VALID_CHANNELS.has(channelName) ? channelName : "nightly";
  },

  get newTabURL() {
    return this._newTabURL;
  },

  get remoteVersion() {
    return REMOTE_NEWTAB_VERSION;
  },

  get remoteReleaseName() {
    return this.releaseFromUpdateChannel(UpdateUtils.UpdateChannel);
  },

  set newTabURL(aNewTabURL) {
    let remoteURL = this.generateRemoteURL();
    let prefRemoteEnabled = Services.prefs.getBoolPref(PREF_REMOTE_ENABLED);
    let isResetLocal = !prefRemoteEnabled && aNewTabURL === LOCAL_NEWTAB_URL;
    let isResetRemote = prefRemoteEnabled && aNewTabURL === remoteURL;

    if (isResetLocal || isResetRemote) {
      if (this._overriden) {
        // only trigger a reset if previously overridden
        this.resetNewTabURL();
      }
      return;
    }
    // turn off remote state if needed
    this.toggleRemote(false);
    this._newTabURL = aNewTabURL;
    this._overridden = true;
    Services.obs.notifyObservers(null, "newtab-url-changed", this._newTabURL);
  },

  get overridden() {
    return this._overridden;
  },

  get remoteEnabled() {
    return this._remoteEnabled;
  },

  resetNewTabURL() {
    this._overridden = false;
    this.toggleRemote(Services.prefs.getBoolPref(PREF_REMOTE_ENABLED), true);
    Services.obs.notifyObservers(null, "newtab-url-changed", this._newTabURL);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AboutNewTabService]);
