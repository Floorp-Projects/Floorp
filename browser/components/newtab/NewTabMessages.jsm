/* global
  NewTabWebChannel,
  NewTabPrefsProvider,
  PlacesProvider,
  PreviewProvider,
  NewTabSearchProvider,
  Preferences,
  XPCOMUtils,
  Task
*/

/* exported NewTabMessages */

"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesProvider",
                                  "resource:///modules/PlacesProvider.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PreviewProvider",
                                  "resource:///modules/PreviewProvider.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabPrefsProvider",
                                  "resource:///modules/NewTabPrefsProvider.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabSearchProvider",
                                  "resource:///modules/NewTabSearchProvider.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabWebChannel",
                                  "resource:///modules/NewTabWebChannel.jsm");

this.EXPORTED_SYMBOLS = ["NewTabMessages"];

const PREF_ENABLED = "browser.newtabpage.remote";
const CURRENT_ENGINE = "browser-search-engine-modified";

// Action names are from the content's perspective. in from chrome == out from content
// Maybe replace the ACTION objects by a bi-directional Map a bit later?
const ACTIONS = {
  inboundActions: [
    "REQUEST_PREFS",
    "REQUEST_THUMB",
    "REQUEST_FRECENT",
    "REQUEST_UISTRINGS",
    "REQUEST_SEARCH_SUGGESTIONS",
    "REQUEST_MANAGE_ENGINES",
    "REQUEST_SEARCH_STATE",
    "REQUEST_REMOVE_FORM_HISTORY",
    "REQUEST_PERFORM_SEARCH",
    "REQUEST_CYCLE_ENGINE",
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
  search: {
    inSearch: {
        UIStrings: "REQUEST_UISTRINGS",
        suggestions: "REQUEST_SEARCH_SUGGESTIONS",
        manageEngines: "REQUEST_MANAGE_ENGINES",
        state: "REQUEST_SEARCH_STATE",
        removeFormHistory: "REQUEST_REMOVE_FORM_HISTORY",
        performSearch: "REQUEST_PERFORM_SEARCH",
        cycleEngine: "REQUEST_CYCLE_ENGINE"
    },
    outSearch: {
      UIStrings: "RECEIVE_UISTRINGS",
      suggestions: "RECEIVE_SEARCH_SUGGESTIONS",
      state: "RECEIVE_SEARCH_STATE",
      currentEngine: "RECEIVE_CURRENT_ENGINE"
    },
  }
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
      case ACTIONS.search.inSearch.UIStrings:
        // Return to the originator all search strings to display
        let strings = NewTabSearchProvider.search.searchSuggestionUIStrings;
        NewTabWebChannel.send(ACTIONS.search.outSearch.UIStrings, strings, target);
        break;
      case ACTIONS.search.inSearch.suggestions:
        // Return to the originator all search suggestions
        Task.spawn(function*() {
          try {
            let {engineName, searchString} = data;
            let suggestions = yield NewTabSearchProvider.search.asyncGetSuggestions(engineName, searchString, target);
            NewTabWebChannel.send(ACTIONS.search.outSearch.suggestions, suggestions, target);
          } catch (e) {
            Cu.reportError(e);
          }
        });
        break;
      case ACTIONS.search.inSearch.manageEngines:
        // Open about:preferences to manage search state
        NewTabSearchProvider.search.manageEngines(target.browser);
        break;
      case ACTIONS.search.inSearch.state:
        // Return the state of the search component (i.e current engine and visible engine details)
        Task.spawn(function*() {
          try {
            let state = yield NewTabSearchProvider.search.asyncGetState();
            NewTabWebChannel.broadcast(ACTIONS.search.outSearch.state, state);
          } catch (e) {
            Cu.reportError(e);
          }
        });
        break;
      case ACTIONS.search.inSearch.removeFormHistory:
        // Remove a form history entry from the search component
        let suggestion = data;
        NewTabSearchProvider.search.removeFormHistory(target, suggestion);
        break;
      case ACTIONS.search.inSearch.performSearch:
        // Perform a search
        NewTabSearchProvider.search.asyncPerformSearch(target, data).catch(Cu.reportError);
        break;
      case ACTIONS.search.inSearch.cycleEngine:
        // Set the new current engine
        NewTabSearchProvider.search.asyncCycleEngine(data).catch(Cu.reportError);
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
   * Broadcast current engine has changed to all open newtab pages
   */
  _handleCurrentEngineChange(name, value) { // jshint unused: false
    let engine = value;
    NewTabWebChannel.broadcast(ACTIONS.search.outSearch.currentEngine, engine);
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
    this._handleCurrentEngineChange = this._handleCurrentEngineChange.bind(this);

    PlacesProvider.links.init();
    NewTabPrefsProvider.prefs.init();
    NewTabSearchProvider.search.init();
    NewTabWebChannel.init();

    this._prefs.enabled = Preferences.get(PREF_ENABLED, false);

    if (this._prefs.enabled) {
      for (let action of ACTIONS.inboundActions) {
        NewTabWebChannel.on(action, this.handleContentRequest);
      }

      NewTabPrefsProvider.prefs.on(PREF_ENABLED, this._handleEnabledChange);
      NewTabSearchProvider.search.on(CURRENT_ENGINE, this._handleCurrentEngineChange);

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
      NewTabSearchProvider.search.off(CURRENT_ENGINE, this._handleCurrentEngineChange);

      for (let action of ACTIONS.inboundActions) {
        NewTabWebChannel.off(action, this.handleContentRequest);
      }

      for (let pref of NewTabPrefsProvider.newtabPagePrefSet) {
        NewTabPrefsProvider.prefs.off(pref, this.handlePrefChange);
      }
    }

    PlacesProvider.links.uninit();
    NewTabPrefsProvider.prefs.uninit();
    NewTabSearchProvider.search.uninit();
    NewTabWebChannel.uninit();
  }
};
