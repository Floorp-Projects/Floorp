 /* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/*globals Components, Services, XPCOMUtils, Task, SearchSuggestionController, PrivateBrowsingUtils, FormHistory*/

"use strict";

this.EXPORTED_SYMBOLS = [
  "SearchProvider",
];

const {
  classes: Cc,
  interfaces: Ci,
  utils: Cu,
} = Components;

Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.importGlobalProperties(["URL", "Blob"]);

XPCOMUtils.defineLazyModuleGetter(this, "FormHistory",
  "resource://gre/modules/FormHistory.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SearchSuggestionController",
  "resource://gre/modules/SearchSuggestionController.jsm");

const stringBundle = Services.strings.createBundle("chrome://global/locale/autocomplete.properties");
const searchBundle = Services.strings.createBundle("chrome://browser/locale/search.properties");
const MAX_LOCAL_SUGGESTIONS = 3;
const MAX_SUGGESTIONS = 6;
// splits data urls into component parts [url, type, data]
const dataURLParts = /(?:^data:)(.+)(?:;base64,)(.*)$/;
const l10nKeysNames = [
  "searchHeader",
  "searchPlaceholder",
  "searchWithHeader",
  "searchSettings",
  "searchForSomethingWith",
];

this.SearchProvider = {
  // This is used to handle search suggestions.  It maps xul:browsers to objects
  // { controller, previousFormHistoryResult }.  See getSuggestions().
  _suggestionMap: new WeakMap(),

  _searchSuggestionUIStrings: new Map(),

  /**
   * Makes a copy of the current search suggestions.
   * @return {Object} Key/value pairs representing the search suggestions.
   */
  get searchSuggestionUIStrings() {
    let result = Object.create(null);
    Array.from(this._searchSuggestionUIStrings.entries())
      .forEach(([key, value]) => result[key] = value);
    return result;
  },

  /**
   * Gets the state
   * @return {Promise} Resolves to an object:
   *      engines {Object[]}:  list of engines.
   *      currentEngine {Object}: the current engine.
   */
  get state() {
    return Task.spawn(function* () {
      let state = {
        engines: [],
        currentEngine: yield this.currentEngine,
      };
      let pref = Services.prefs.getCharPref("browser.search.hiddenOneOffs");
      let hiddenList = pref ? pref.split(",") : [];
      for (let engine of Services.search.getVisibleEngines()) {
        if (hiddenList.indexOf(engine.name) !== -1) {
          continue;
        }
        let uri = engine.getIconURLBySize(16, 16);
        state.engines.push({
          name: engine.name,
          iconBuffer: yield this._arrayBufferFromDataURL(uri),
        });
      }
      return state;
    }.bind(this));
  },

  /**
   * Get a browser to peform a search by opening a new window.
   * @param  {XULBrowser} browser The browser that performs the search.
   * @param  {Object} data The data used to perform the search.
   * @return {Window} win The window that is performing the search.
   */
  performSearch(browser, data) {
    return Task.spawn(function* () {
      let engine = Services.search.getEngineByName(data.engineName);
      let submission = engine.getSubmission(data.searchString, "", data.searchPurpose);
      // The browser may have been closed between the time its content sent the
      // message and the time we handle it. In that case, trying to call any
      // method on it will throw.
      let win = browser.ownerDocument.defaultView;

      let where = win.whereToOpenLink(data.originalEvent);

      // There is a chance that by the time we receive the search message, the user
      // has switched away from the tab that triggered the search. If, based on the
      // event, we need to load the search in the same tab that triggered it (i.e.
      // where == "current"), openUILinkIn will not work because that tab is no
      // longer the current one. For this case we manually load the URI.
      if (where === "current") {
        browser.loadURIWithFlags(submission.uri.spec,
          Ci.nsIWebNavigation.LOAD_FLAGS_NONE, null, null,
          submission.postData);
      } else {
        let params = {
          postData: submission.postData,
          inBackground: Services.prefs.getBoolPref("browser.tabs.loadInBackground"),
        };
        win.openUILinkIn(submission.uri.spec, where, params);
      }
      win.BrowserSearch.recordSearchInHealthReport(engine, data.healthReportKey,
        data.selection || null);

      yield this.addFormHistoryEntry(browser, data.searchString);
      return win;
    }.bind(this));
  },

  /**
   * Returns the current search engine.
   * @return {Object} An object the describes the current engine.
   */
  get currentEngine() {
    return Task.spawn(function* () {
      let engine = Services.search.currentEngine;
      let favicon = engine.getIconURLBySize(16, 16);
      let uri1x = engine.getIconURLBySize(65, 26);
      let uri2x = engine.getIconURLBySize(130, 52);
      let placeholder = stringBundle.formatStringFromName(
        "searchWithEngine", [engine.name], 1);
      let obj = {
        name: engine.name,
        placeholder: placeholder,
        iconBuffer: yield this._arrayBufferFromDataURL(favicon),
        logoBuffer: yield this._arrayBufferFromDataURL(uri1x),
        logo2xBuffer: yield this._arrayBufferFromDataURL(uri2x),
        preconnectOrigin: new URL(engine.searchForm).origin,
      };
      return obj;
    }.bind(this));
  },

  getSuggestions: function(browser, data) {
    return Task.spawn(function* () {
      let engine = Services.search.getEngineByName(data.engineName);
      if (!engine) {
        throw new Error(`Unknown engine name: ${data.engineName}`);
      }
      let {
        controller
      } = this._suggestionDataForBrowser(browser);
      let ok = SearchSuggestionController.engineOffersSuggestions(engine);
      controller.maxLocalResults = ok ? MAX_LOCAL_SUGGESTIONS : MAX_SUGGESTIONS;
      controller.maxRemoteResults = ok ? MAX_SUGGESTIONS : 0;
      controller.remoteTimeout = data.remoteTimeout || undefined;
      let isPrivate = PrivateBrowsingUtils.isBrowserPrivate(browser);

      let suggestions;
      try {
        // If fetch() rejects due to it's asynchronous behaviour, the suggestions
        // is null and is then handled.
        suggestions = yield controller.fetch(data.searchString, isPrivate, engine);
      } catch (e) {
        Cu.reportError(e);
      }

      let result = null;
      if (suggestions) {
        this._suggestionMap.get(browser)
          .previousFormHistoryResult = suggestions.formHistoryResult;

        result = {
          engineName: data.engineName,
          searchString: suggestions.term,
          formHistory: suggestions.local,
          remote: suggestions.remote,
        };
    }

    return result;
    }.bind(this));
  },

  addFormHistoryEntry: function(browser, entry = "") {
    return Task.spawn(function* () {
      let isPrivate = false;
      try {
        isPrivate = PrivateBrowsingUtils.isBrowserPrivate(browser);
      } catch (err) {
        // The browser might have already been destroyed.
        return false;
      }
      if (isPrivate || entry === "") {
        return false;
      }
      let {
        controller
      } = this._suggestionDataForBrowser(browser);
      let result = yield new Promise((resolve, reject) => {
        let ops = {
          op: "bump",
          fieldname: controller.formHistoryParam,
          value: entry,
        };
        let callbacks = {
          handleCompletion: () => resolve(true),
          handleError: reject,
        };
        FormHistory.update(ops, callbacks);
      });
      return result;
    }.bind(this));
  },

  /**
   * Removes an entry from the form history for a given browser.
   *
   * @param  {XULBrowser} browser the browser to delete from.
   * @param  {String} suggestion The suggestion to delete.
   * @return {Boolean} True if removed, false otherwise.
   */
  removeFormHistoryEntry(browser, suggestion) {
    let {
      previousFormHistoryResult
    } = this._suggestionMap.get(browser);
    if (!previousFormHistoryResult) {
      return false;
    }
    for (let i = 0; i < previousFormHistoryResult.matchCount; i++) {
      if (previousFormHistoryResult.getValueAt(i) === suggestion) {
        previousFormHistoryResult.removeValueAt(i, true);
        return true;
      }
    }
    return false;
  },

  _suggestionDataForBrowser(browser) {
    let data = this._suggestionMap.get(browser);
    if (!data) {
      // Since one SearchSuggestionController instance is meant to be used per
      // autocomplete widget, this means that we assume each xul:browser has at
      // most one such widget.
      data = {
        controller: new SearchSuggestionController(),
        previousFormHistoryResult: undefined,
      };
      this._suggestionMap.set(browser, data);
    }
    return data;
  },

  _arrayBufferFromDataURL(dataURL = "") {
    if (!dataURL) {
      return Promise.resolve(null);
    }
    return new Promise((resolve, reject) => {
      try {
        let fileReader = Cc["@mozilla.org/files/filereader;1"]
          .createInstance(Ci.nsIDOMFileReader);
        let [type, data] = dataURLParts.exec(dataURL).slice(1);
        let bytes = atob(data);
        let uInt8Array = new Uint8Array(bytes.length);
        for (let i = 0; i < bytes.length; ++i) {
          uInt8Array[i] = bytes.charCodeAt(i);
        }
        let blob = new Blob([uInt8Array], {
          type
        });
        fileReader.onload = () => resolve(fileReader.result);
        fileReader.onerror = () => resolve(null);
        fileReader.readAsArrayBuffer(blob);
      } catch (e) {
        reject(e);
      }
    });
  },

  init() {
    // Perform localization
    l10nKeysNames.map(
      name => [name, searchBundle.GetStringFromName(name)]
    ).forEach(
      ([key, value]) => this._searchSuggestionUIStrings.set(key, value)
    );
  }
};
this.SearchProvider.init();
