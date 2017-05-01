/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/* globals XPCOMUtils, Preferences, Services */
"use strict";

const {utils: Cu, interfaces: Ci} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

const LOCAL_NEWTAB_URL = "chrome://browser/content/newtab/newTab.xhtml";

const ACTIVITY_STREAM_URL = "resource://activity-stream/data/content/activity-stream.html";

const ABOUT_URL = "about:newtab";

// Pref that tells if activity stream is enabled
const PREF_ACTIVITY_STREAM_ENABLED = "browser.newtabpage.activity-stream.enabled";

function AboutNewTabService() {
  Preferences.observe(PREF_ACTIVITY_STREAM_ENABLED, this._handleToggleEvent.bind(this));
  this.toggleActivityStream(Services.prefs.getBoolPref(PREF_ACTIVITY_STREAM_ENABLED));
}

/*
 * A service that allows for the overriding, at runtime, of the newtab page's url.
 * Additionally, the service manages pref state between a activity stream, or the regular
 * about:newtab page.
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
 * two URLs, a chrome or the activity stream one, based on the
 * browser.newtabpage.activity-stream.enabled pref.
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
  _activityStreamEnabled: false,
  _overridden: false,

  classID: Components.ID("{dfcd2adc-7867-4d3a-ba70-17501f208142}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAboutNewTabService]),
  _xpcom_categories: [{
    service: true
  }],

  _handleToggleEvent(stateEnabled) {
    if (this.toggleActivityStream(stateEnabled)) {
      Services.obs.notifyObservers(null, "newtab-url-changed", ABOUT_URL);
    }
  },

  /**
   * React to changes to the activity stream pref.
   *
   * If browser.newtabpage.activity-stream.enabled is true, this will change the default URL to the
   * activity stream page URL. If browser.newtabpage.activity-stream.enabled is false, the default URL
   * will be a local chrome URL.
   *
   * This will only act if there is a change of state and if not overridden.
   *
   * @returns {Boolean} Returns if there has been a state change
   *
   * @param {Boolean}   stateEnabled    activity stream enabled state to set to
   * @param {Boolean}   forceState      force state change
   */
  toggleActivityStream(stateEnabled, forceState = false) {

    if (!forceState && (this.overridden || stateEnabled === this.activityStreamEnabled)) {
      // exit there is no change of state
      return false;
    }
    if (stateEnabled) {
      this._activityStreamEnabled = true;
    } else {
      this._activityStreamEnabled = false;
    }
    this._newtabURL = ABOUT_URL;
    return true;
  },

  /*
   * Returns the default URL.
   *
   * This URL only depends on the browser.newtabpage.activity-stream.enabled pref. Overriding
   * the newtab page has no effect on the result of this function.
   *
   * @returns {String} the default newtab URL, activity-stream or regular depending on browser.newtabpage.activity-stream.enabled
   */
  get defaultURL() {
    if (this.activityStreamEnabled) {
      return this.activityStreamURL;
    }
    return LOCAL_NEWTAB_URL;
  },

  get newTabURL() {
    return this._newTabURL;
  },

  set newTabURL(aNewTabURL) {
    aNewTabURL = aNewTabURL.trim();
    if (aNewTabURL === ABOUT_URL) {
      // avoid infinite redirects in case one sets the URL to about:newtab
      this.resetNewTabURL();
      return;
    } else if (aNewTabURL === "") {
      aNewTabURL = "about:blank";
    }

    this.toggleActivityStream(false);
    this._newTabURL = aNewTabURL;
    this._overridden = true;
    Services.obs.notifyObservers(null, "newtab-url-changed", this._newTabURL);
  },

  get overridden() {
    return this._overridden;
  },

  get activityStreamEnabled() {
    return this._activityStreamEnabled;
  },

  get activityStreamURL() {
    return ACTIVITY_STREAM_URL;
  },

  resetNewTabURL() {
    this._overridden = false;
    this._newTabURL = ABOUT_URL;
    this.toggleActivityStream(Services.prefs.getBoolPref(PREF_ACTIVITY_STREAM_ENABLED), true);
    Services.obs.notifyObservers(null, "newtab-url-changed", this._newTabURL);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AboutNewTabService]);
