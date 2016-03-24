/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/* globals XPCOMUtils, NewTabPrefsProvider, Services,
  Locale, UpdateUtils, NewTabRemoteResources
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
XPCOMUtils.defineLazyModuleGetter(this, "NewTabRemoteResources",
                                  "resource:///modules/NewTabRemoteResources.jsm");

const LOCAL_NEWTAB_URL = "chrome://browser/content/newtab/newTab.xhtml";

const REMOTE_NEWTAB_PATH = "/newtab/v%VERSION%/%CHANNEL%/%LOCALE%/index.html";

const ABOUT_URL = "about:newtab";

// Pref that tells if remote newtab is enabled
const PREF_REMOTE_ENABLED = "browser.newtabpage.remote";

// Pref branch necesssary for testing
const PREF_REMOTE_CS_TEST = "browser.newtabpage.remote.content-signing-test";

// The preference that tells whether to match the OS locale
const PREF_MATCH_OS_LOCALE = "intl.locale.matchOS";

// The preference that tells what locale the user selected
const PREF_SELECTED_LOCALE = "general.useragent.locale";

// The preference that tells what remote mode is enabled.
const PREF_REMOTE_MODE = "browser.newtabpage.remote.mode";

// The preference that tells which remote version is expected.
const PREF_REMOTE_VERSION = "browser.newtabpage.remote.version";

const VALID_CHANNELS = new Set(["esr", "release", "beta", "aurora", "nightly"]);

function AboutNewTabService() {
  NewTabPrefsProvider.prefs.on(PREF_REMOTE_ENABLED, this._handleToggleEvent.bind(this));

  this._updateRemoteMaybe = this._updateRemoteMaybe.bind(this);

  // trigger remote change if needed, according to pref
  this.toggleRemote(Services.prefs.getBoolPref(PREF_REMOTE_ENABLED));
}

/*
 * A service that allows for the overriding, at runtime, of the newtab page's url.
 * Additionally, the service manages pref state between a remote and local newtab page.
 *
 * There is tight coupling with browser/about/AboutRedirector.cpp.
 *
 * 1. Browser chrome access:
 *
 * When the user issues a command to open a new tab page, usually clicking a button
 * in the browser chrome or using shortcut keys, the browser chrome code invokes the
 * service to obtain the newtab URL. It then loads that URL in a new tab.
 *
 * When not overridden, the default URL emitted by the service is "about:newtab".
 * When overridden, it returns the overriden URL.
 *
 * 2. Redirector Access:
 *
 * When the URL loaded is about:newtab, the default behavior, or when entered in the
 * URL bar, the redirector is hit. The service is then called to return either of
 * two URLs, a chrome or remote one, based on the browser.newtabpage.remote pref.
 *
 * NOTE: "about:newtab" will always result in a default newtab page, and never an overridden URL.
 *
 * Access patterns:
 *
 * The behavior is different when accessing the service via browser chrome or via redirector
 * largely to maintain compatibility with expectations of add-on developers.
 *
 * Loading a chrome resource, or an about: URL in the redirector with either the
 * LOAD_NORMAL or LOAD_REPLACE flags yield unexpected behaviors, so a roundtrip
 * to the redirector from browser chrome is avoided.
 */
AboutNewTabService.prototype = {

  _newTabURL: ABOUT_URL,
  _remoteEnabled: false,
  _remoteURL: null,
  _overridden: false,

  classID: Components.ID("{dfcd2adc-7867-4d3a-ba70-17501f208142}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAboutNewTabService]),
  _xpcom_categories: [{
    service: true
  }],

  _handleToggleEvent(prefName, stateEnabled, forceState) { //jshint unused:false
    if (this.toggleRemote(stateEnabled, forceState)) {
      Services.obs.notifyObservers(null, "newtab-url-changed", ABOUT_URL);
    }
  },

  /**
   * React to changes to the remote newtab pref.
   *
   * If browser.newtabpage.remote is true, this will change the default URL to the
   * remote newtab page URL. If browser.newtabpage.remote is false, the default URL
   * will be a local chrome URL.
   *
   * This will only act if there is a change of state and if not overridden.
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

    let csTest = Services.prefs.getBoolPref(PREF_REMOTE_CS_TEST);
    if (stateEnabled) {
      if (!csTest) {
        this._remoteURL = this.generateRemoteURL();
      } else {
        this._remoteURL = this._newTabURL;
      }
      NewTabPrefsProvider.prefs.on(
        PREF_SELECTED_LOCALE,
        this._updateRemoteMaybe);
      NewTabPrefsProvider.prefs.on(
        PREF_MATCH_OS_LOCALE,
        this._updateRemoteMaybe);
      NewTabPrefsProvider.prefs.on(
        PREF_REMOTE_MODE,
        this._updateRemoteMaybe);
      NewTabPrefsProvider.prefs.on(
        PREF_REMOTE_VERSION,
        this._updateRemoteMaybe);
      this._remoteEnabled = true;
    } else {
      NewTabPrefsProvider.prefs.off(PREF_SELECTED_LOCALE, this._updateRemoteMaybe);
      NewTabPrefsProvider.prefs.off(PREF_MATCH_OS_LOCALE, this._updateRemoteMaybe);
      NewTabPrefsProvider.prefs.off(PREF_REMOTE_MODE, this._updateRemoteMaybe);
      NewTabPrefsProvider.prefs.off(PREF_REMOTE_VERSION, this._updateRemoteMaybe);
      this._remoteEnabled = false;
    }
    if (!csTest) {
      this._newTabURL = ABOUT_URL;
    }
    return true;
  },

  /*
   * Generate a default url based on remote mode, version, locale and update channel
   */
  generateRemoteURL() {
    let releaseName = this.releaseFromUpdateChannel(UpdateUtils.UpdateChannel);
    let path = REMOTE_NEWTAB_PATH
      .replace("%VERSION%", this.remoteVersion)
      .replace("%LOCALE%", Locale.getLocale())
      .replace("%CHANNEL%", releaseName);
    let mode = Services.prefs.getCharPref(PREF_REMOTE_MODE, "production");
    if (!(mode in NewTabRemoteResources.MODE_CHANNEL_MAP)) {
      mode = "production";
    }
    return NewTabRemoteResources.MODE_CHANNEL_MAP[mode].origin + path;
  },

  /*
   * Returns the default URL.
   *
   * This URL only depends on the browser.newtabpage.remote pref. Overriding
   * the newtab page has no effect on the result of this function.
   *
   * The result is also the remote URL if this is in a test (PREF_REMOTE_CS_TEST)
   *
   * @returns {String} the default newtab URL, remote or local depending on browser.newtabpage.remote
   */
  get defaultURL() {
    let csTest = Services.prefs.getBoolPref(PREF_REMOTE_CS_TEST);
    if (this._remoteEnabled || csTest)  {
      return this._remoteURL;
    }
    return LOCAL_NEWTAB_URL;
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
    if (url !== this._remoteURL) {
      this._remoteURL = url;
      Services.obs.notifyObservers(null, "newtab-url-changed",
        this._remoteURL);
    }
  },

  /**
   * Returns the release name from an Update Channel name
   *
   * @returns {String} a release name based on the update channel. Defaults to nightly
   */
  releaseFromUpdateChannel(channelName) {
    return VALID_CHANNELS.has(channelName) ? channelName : "nightly";
  },

  get newTabURL() {
    return this._newTabURL;
  },

  get remoteVersion() {
    return Services.prefs.getCharPref(PREF_REMOTE_VERSION, "1");
  },

  get remoteReleaseName() {
    return this.releaseFromUpdateChannel(UpdateUtils.UpdateChannel);
  },

  set newTabURL(aNewTabURL) {
    let csTest = Services.prefs.getBoolPref(PREF_REMOTE_CS_TEST);
    aNewTabURL = aNewTabURL.trim();
    if (aNewTabURL === ABOUT_URL) {
      // avoid infinite redirects in case one sets the URL to about:newtab
      this.resetNewTabURL();
      return;
    } else if (aNewTabURL === "") {
      aNewTabURL = "about:blank";
    }
    let remoteURL = this.generateRemoteURL();
    let prefRemoteEnabled = Services.prefs.getBoolPref(PREF_REMOTE_ENABLED);
    let isResetLocal = !prefRemoteEnabled && aNewTabURL === LOCAL_NEWTAB_URL;
    let isResetRemote = prefRemoteEnabled && aNewTabURL === remoteURL;

    if (isResetLocal || isResetRemote) {
      if (this._overriden && !csTest) {
        // only trigger a reset if previously overridden and this is no test
        this.resetNewTabURL();
      }
      return;
    }
    // turn off remote state if needed
    if (!csTest) {
      this.toggleRemote(false);
    } else {
      // if this is a test, we want the remoteURL to be set
      this._remoteURL = aNewTabURL;
    }
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
    this._newTabURL = ABOUT_URL;
    this.toggleRemote(Services.prefs.getBoolPref(PREF_REMOTE_ENABLED), true);
    Services.obs.notifyObservers(null, "newtab-url-changed", this._newTabURL);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AboutNewTabService]);
