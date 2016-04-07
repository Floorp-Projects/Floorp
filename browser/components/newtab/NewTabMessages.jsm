/*global
  NewTabWebChannel,
  NewTabPrefsProvider,
  PlacesProvider,
  Preferences,
  XPCOMUtils
*/

/* exported NewTabMessages */

"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesProvider",
                                  "resource:///modules/PlacesProvider.jsm");
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
  inboundActions: [
    "REQUEST_PREFS",
    "REQUEST_THUMB",
    "REQUEST_FRECENT",
  ],
  prefs: {
    inPrefs: "REQUEST_PREFS",
    outPrefs: "RECEIVE_PREFS",
  },
  preview: {
    inThumb: "REQUEST_THUMB",
    outThumb: "RECEIVE_THUMB",
  },
  links: {
    inFrecent: "REQUEST_FRECENT",
    outFrecent: "RECEIVE_FRECENT",
    outPlacesChange: "RECEIVE_PLACES_CHANGE",
  },
};

let NewTabMessages = {

  _prefs: {},

  /** NEWTAB EVENT HANDLERS **/

  handleContentRequest(actionName, {data, target}) {
    switch (actionName) {
      case ACTIONS.prefs.inPrefs:
        // Return to the originator all newtabpage prefs
        let results = NewTabPrefsProvider.prefs.newtabPagePrefs;
        NewTabWebChannel.send(ACTIONS.prefs.outPrefs, results, target);
        break;
      case ACTIONS.preview.inThumb:
        // Return to the originator a preview URL
        PreviewProvider.getThumbnail(data).then(imgData => {
          NewTabWebChannel.send(ACTIONS.preview.outThumb, {url: data, imgData}, target);
        });
        break;
      case ACTIONS.links.inFrecent:
        // Return to the originator the top frecent links
        PlacesProvider.links.getLinks().then(links => {
          NewTabWebChannel.send(ACTIONS.links.outFrecent, links, target);
        });
        break;
    }
  },

  /*
   * Broadcast places change to all open newtab pages
   */
  handlePlacesChange(type, data) {
    NewTabWebChannel.broadcast(ACTIONS.links.outPlacesChange, {type, data});
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
    this.handleContentRequest = this.handleContentRequest.bind(this);
    this._handleEnabledChange = this._handleEnabledChange.bind(this);

    PlacesProvider.links.init();
    NewTabPrefsProvider.prefs.init();
    NewTabWebChannel.init();

    this._prefs.enabled = Preferences.get(PREF_ENABLED, false);

    if (this._prefs.enabled) {
      for (let action of ACTIONS.inboundActions) {
        NewTabWebChannel.on(action, this.handleContentRequest);
      }
      NewTabPrefsProvider.prefs.on(PREF_ENABLED, this._handleEnabledChange);

      for (let pref of NewTabPrefsProvider.newtabPagePrefSet) {
        NewTabPrefsProvider.prefs.on(pref, this.handlePrefChange);
      }

      PlacesProvider.links.on("deleteURI", this.handlePlacesChange);
      PlacesProvider.links.on("clearHistory", this.handlePlacesChange);
      PlacesProvider.links.on("linkChanged", this.handlePlacesChange);
      PlacesProvider.links.on("manyLinksChanged", this.handlePlacesChange);
    }
  },

  uninit() {
    this._prefs.enabled = Preferences.get(PREF_ENABLED, false);

    if (this._prefs.enabled) {
      NewTabPrefsProvider.prefs.off(PREF_ENABLED, this._handleEnabledChange);

      for (let action of ACTIONS.inboundActions) {
        NewTabWebChannel.off(action, this.handleContentRequest);
      }

      for (let pref of NewTabPrefsProvider.newtabPagePrefSet) {
        NewTabPrefsProvider.prefs.off(pref, this.handlePrefChange);
      }
    }

    PlacesProvider.links.uninit();
    NewTabPrefsProvider.prefs.uninit();
    NewTabWebChannel.uninit();
  }
};
