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

XPCOMUtils.defineLazyModuleGetter(this, "PreviewProvider",
                                  "resource:///modules/PreviewProvider.jsm");
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
    action_types: new Set(["REQUEST_PREFS"]),
  },
  preview: {
    inThumb: "REQUEST_THUMB",
    outThumb: "RECEIVE_THUMB",
    action_types: new Set(["REQUEST_THUMB"]),
  },
};

let NewTabMessages = {

  _prefs: {},

  /** NEWTAB EVENT HANDLERS **/

  /*
   * Return to the originator all newtabpage prefs. A point-to-point request.
   */
  handlePrefRequest(actionName, {target}) {
    if (ACTIONS.prefs.inPrefs === actionName) {
      let results = NewTabPrefsProvider.prefs.newtabPagePrefs;
      NewTabWebChannel.send(ACTIONS.prefs.outPrefs, results, target);
    }
  },

  handlePreviewRequest(actionName, {data, target}) {
    if (ACTIONS.preview.inThumb === actionName) {
      PreviewProvider.getThumbnail(data).then(imgData => {
        NewTabWebChannel.send(ACTIONS.preview.outThumb, {url: data, imgData}, target);
      });
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
    this.handlePrefRequest = this.handlePrefRequest.bind(this);
    this.handlePreviewRequest = this.handlePreviewRequest.bind(this);
    this.handlePrefChange = this.handlePrefChange.bind(this);
    this._handleEnabledChange = this._handleEnabledChange.bind(this);

    NewTabPrefsProvider.prefs.init();
    NewTabWebChannel.init();

    this._prefs.enabled = Preferences.get(PREF_ENABLED, false);

    if (this._prefs.enabled) {
      NewTabWebChannel.on(ACTIONS.prefs.inPrefs, this.handlePrefRequest);
      NewTabWebChannel.on(ACTIONS.preview.inThumb, this.handlePreviewRequest);

      NewTabPrefsProvider.prefs.on(PREF_ENABLED, this._handleEnabledChange);

      for (let pref of NewTabPrefsProvider.newtabPagePrefSet) {
        NewTabPrefsProvider.prefs.on(pref, this.handlePrefChange);
      }
    }
  },

  uninit() {
    this._prefs.enabled = Preferences.get(PREF_ENABLED, false);

    if (this._prefs.enabled) {
      NewTabPrefsProvider.prefs.off(PREF_ENABLED, this._handleEnabledChange);

      NewTabWebChannel.off(ACTIONS.prefs.inPrefs, this.handlePrefRequest);
      NewTabWebChannel.off(ACTIONS.prefs.inThumb, this.handlePreviewRequest);
      for (let pref of NewTabPrefsProvider.newtabPagePrefSet) {
        NewTabPrefsProvider.prefs.off(pref, this.handlePrefChange);
      }
    }

    NewTabPrefsProvider.prefs.uninit();
    NewTabWebChannel.uninit();
  }
};
