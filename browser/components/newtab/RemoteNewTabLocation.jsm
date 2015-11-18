/* globals Services, UpdateUtils, XPCOMUtils, URL, NewTabPrefsProvider, Locale */
/* exported RemoteNewTabLocation */

"use strict";

this.EXPORTED_SYMBOLS = ["RemoteNewTabLocation"];

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.importGlobalProperties(["URL"]);

XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabPrefsProvider",
  "resource:///modules/NewTabPrefsProvider.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Locale",
  "resource://gre/modules/Locale.jsm");

// The preference that tells whether to match the OS locale
const PREF_MATCH_OS_LOCALE = "intl.locale.matchOS";

// The preference that tells what locale the user selected
const PREF_SELECTED_LOCALE = "general.useragent.locale";

const DEFAULT_PAGE_LOCATION = "https://newtab.cdn.mozilla.net/" +
                              "v%VERSION%/%CHANNEL%/%LOCALE%/index.html";

const VALID_CHANNELS = new Set(["esr", "release", "beta", "aurora", "nightly"]);

const NEWTAB_VERSION = "0";

let RemoteNewTabLocation = {
  /*
   * Generate a default url based on locale and update channel
   */
  _generateDefaultURL() {
    let releaseName = this._releaseFromUpdateChannel(UpdateUtils.UpdateChannel);
    let uri = DEFAULT_PAGE_LOCATION
      .replace("%VERSION%", this.version)
      .replace("%LOCALE%", Locale.getLocale())
      .replace("%CHANNEL%", releaseName);
    return new URL(uri);
  },

  _url: null,
  _overridden: false,

  get href() {
    return this._url.href;
  },

  get origin() {
    return this._url.origin;
  },

  get overridden() {
    return this._overridden;
  },

  get version() {
    return NEWTAB_VERSION;
  },

  get channels() {
    return VALID_CHANNELS;
  },

  /**
   * Returns the release name from an Update Channel name
   *
   * @return {String} a release name based on the update channel. Defaults to nightly
   */
  _releaseFromUpdateChannel(channel) {
    let result = "nightly";
    if (VALID_CHANNELS.has(channel)) {
      result = channel;
    }
    return result;
  },

  /*
   * Updates the location when the page is not overriden.
   * Useful when there is a pref change
   */
  _updateMaybe() {
    if (!this.overridden) {
      let url = this._generateDefaultURL();
      if (url.href !== this._url.href) {
        this._url = url;
        Services.obs.notifyObservers(null, "remote-new-tab-location-changed",
          this._url.href);
      }
    }
  },

  /*
   * Override the Remote newtab page location.
   */
  override(newURL) {
    let url = new URL(newURL);
    if (url.href !== this._url.href) {
      this._overridden = true;
      this._url = url;
      Services.obs.notifyObservers(null, "remote-new-tab-location-changed",
                                   this._url.href);
    }
  },

  /*
   * Reset the newtab page location to the default value
   */
  reset() {
    let url = this._generateDefaultURL();
    if (url.href !== this._url.href) {
      this._url = url;
      this._overridden = false;
      Services.obs.notifyObservers(null, "remote-new-tab-location-changed",
        this._url.href);
    }
  },

  init() {
    NewTabPrefsProvider.prefs.on(
      PREF_SELECTED_LOCALE,
      this._updateMaybe.bind(this));

    NewTabPrefsProvider.prefs.on(
      PREF_MATCH_OS_LOCALE,
      this._updateMaybe.bind(this));

    this._url = this._generateDefaultURL();
  },

  uninit() {
    this._url = null;
    this._overridden = false;
    NewTabPrefsProvider.prefs.off(PREF_SELECTED_LOCALE, this._updateMaybe);
    NewTabPrefsProvider.prefs.off(PREF_MATCH_OS_LOCALE, this._updateMaybe);
  }
};
