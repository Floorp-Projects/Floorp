/*global
  NewTabWebChannel,
  NewTabPrefsProvider,
  Preferences,
  XPCOMUtils
*/

/* exported NewTabMessages */

"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NewTabPrefsProvider",
                                  "resource:///modules/NewTabPrefsProvider.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabWebChannel",
                                  "resource:///modules/NewTabWebChannel.jsm");

this.EXPORTED_SYMBOLS = ["NewTabMessages"];

const PREF_ENABLED = "browser.newtabpage.remote";

// Action names are from the content's perspective. in from chrome == out from content
// Maybe replace the ACTION objects by a bi-directional Map a bit later?
const ACTIONS = {
  prefs: {
    inPrefs: "REQUEST_PREFS",
    outPrefs: "RECEIVE_PREFS",
    action_types: new Set(["REQUEST_PREFS", "RECEIVE_PREFS"]),
  }
};

let NewTabMessages = {

  _prefs: {},

  /** NEWTAB EVENT HANDLERS **/

  /*
   * Return to the originator all newtabpage prefs. A point-to-point request.
   */
  handlePrefRequest(actionName, {target}) {
    if (ACTIONS.prefs.action_types.has(actionName)) {
      let results = NewTabPrefsProvider.prefs.newtabPagePrefs;
      NewTabWebChannel.send(ACTIONS.prefs.outPrefs, results, target);
    }
  },

  /*
   * Broadcast preference changes to all open newtab pages
   */
  handlePrefChange(actionName, value) {
    let prefChange = {};
    prefChange[actionName] = value;
    NewTabWebChannel.broadcast(ACTIONS.prefs.outPrefs, prefChange);
  },

  _handleEnabledChange(prefName, value) {
    if (prefName === PREF_ENABLED) {
      if (this._prefs.enabled && !value) {
        this.uninit();
      } else if (!this._prefs.enabled && value) {
        this.init();
      }
    }
  },

  init() {
    NewTabPrefsProvider.prefs.init();
    NewTabWebChannel.init();

    this._prefs.enabled = Preferences.get(PREF_ENABLED, false);

    if (this._prefs.enabled) {
      NewTabWebChannel.on(ACTIONS.prefs.inPrefs, this.handlePrefRequest.bind(this));
      NewTabPrefsProvider.prefs.on(PREF_ENABLED, this._handleEnabledChange.bind(this));

      for (let pref of NewTabPrefsProvider.newtabPagePrefSet) {
        NewTabPrefsProvider.prefs.on(pref, this.handlePrefChange.bind(this));
      }
    }
  },

  uninit() {
    this._prefs.enabled = Preferences.get(PREF_ENABLED, false);

    if (this._prefs.enabled) {
      NewTabPrefsProvider.prefs.off(PREF_ENABLED, this._handleEnabledChange);

      NewTabWebChannel.off(ACTIONS.prefs.inPrefs, this.handlePrefRequest);
      for (let pref of NewTabPrefsProvider.newtabPagePrefSet) {
        NewTabPrefsProvider.prefs.off(pref, this.handlePrefChange);
      }
    }

    NewTabPrefsProvider.prefs.uninit();
    NewTabWebChannel.uninit();
  }
};
