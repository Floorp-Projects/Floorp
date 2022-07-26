/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ContentSearchParent", "ContentSearch"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserSearchTelemetry: "resource:///modules/BrowserSearchTelemetry.sys.mjs",

  SearchSuggestionController:
    "resource://gre/modules/SearchSuggestionController.sys.mjs",

  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  FormHistory: "resource://gre/modules/FormHistory.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
});

const MAX_LOCAL_SUGGESTIONS = 3;
const MAX_SUGGESTIONS = 6;
const SEARCH_ENGINE_PLACEHOLDER_ICON =
  "chrome://browser/skin/search-engine-placeholder.png";

// Set of all ContentSearch actors, used to broadcast messages to all of them.
let gContentSearchActors = new Set();

/**
 * Inbound messages have the following types:
 *
 *   AddFormHistoryEntry
 *     Adds an entry to the search form history.
 *     data: the entry, a string
 *   GetSuggestions
 *     Retrieves an array of search suggestions given a search string.
 *     data: { engineName, searchString }
 *   GetState
 *     Retrieves the current search engine state.
 *     data: null
 *   GetStrings
 *     Retrieves localized search UI strings.
 *     data: null
 *   ManageEngines
 *     Opens the search engine management window.
 *     data: null
 *   RemoveFormHistoryEntry
 *     Removes an entry from the search form history.
 *     data: the entry, a string
 *   Search
 *     Performs a search.
 *     Any GetSuggestions messages in the queue from the same target will be
 *     cancelled.
 *     data: { engineName, searchString, healthReportKey, searchPurpose }
 *   SetCurrentEngine
 *     Sets the current engine.
 *     data: the name of the engine
 *   SpeculativeConnect
 *     Speculatively connects to an engine.
 *     data: the name of the engine
 *
 * Outbound messages have the following types:
 *
 *   CurrentEngine
 *     Broadcast when the current engine changes.
 *     data: see _currentEngineObj
 *   CurrentState
 *     Broadcast when the current search state changes.
 *     data: see currentStateObj
 *   State
 *     Sent in reply to GetState.
 *     data: see currentStateObj
 *   Strings
 *     Sent in reply to GetStrings
 *     data: Object containing string names and values for the current locale.
 *   Suggestions
 *     Sent in reply to GetSuggestions.
 *     data: see _onMessageGetSuggestions
 *   SuggestionsCancelled
 *     Sent in reply to GetSuggestions when pending GetSuggestions events are
 *     cancelled.
 *     data: null
 */

let ContentSearch = {
  initialized: false,

  // Inbound events are queued and processed in FIFO order instead of handling
  // them immediately, which would result in non-FIFO responses due to the
  // asynchrononicity added by converting image data URIs to ArrayBuffers.
  _eventQueue: [],
  _currentEventPromise: null,

  // This is used to handle search suggestions.  It maps xul:browsers to objects
  // { controller, previousFormHistoryResult }.  See _onMessageGetSuggestions.
  _suggestionMap: new WeakMap(),

  // Resolved when we finish shutting down.
  _destroyedPromise: null,

  // The current controller and browser in _onMessageGetSuggestions.  Allows
  // fetch cancellation from _cancelSuggestions.
  _currentSuggestion: null,

  init() {
    if (!this.initialized) {
      Services.obs.addObserver(this, "browser-search-engine-modified");
      Services.obs.addObserver(this, "browser-search-service");
      Services.obs.addObserver(this, "shutdown-leaks-before-check");
      Services.prefs.addObserver("browser.search.hiddenOneOffs", this);
      lazy.UrlbarPrefs.addObserver(this);

      this.initialized = true;
    }
  },

  get searchSuggestionUIStrings() {
    if (this._searchSuggestionUIStrings) {
      return this._searchSuggestionUIStrings;
    }
    this._searchSuggestionUIStrings = {};
    let searchBundle = Services.strings.createBundle(
      "chrome://browser/locale/search.properties"
    );
    let stringNames = [
      "searchHeader",
      "searchForSomethingWith2",
      "searchWithHeader",
      "searchSettings",
    ];

    for (let name of stringNames) {
      this._searchSuggestionUIStrings[name] = searchBundle.GetStringFromName(
        name
      );
    }
    return this._searchSuggestionUIStrings;
  },

  destroy() {
    if (!this.initialized) {
      return new Promise();
    }

    if (this._destroyedPromise) {
      return this._destroyedPromise;
    }

    Services.prefs.removeObserver("browser.search.hiddenOneOffs", this);
    Services.obs.removeObserver(this, "browser-search-engine-modified");
    Services.obs.removeObserver(this, "browser-search-service");
    Services.obs.removeObserver(this, "shutdown-leaks-before-check");

    this._eventQueue.length = 0;
    this._destroyedPromise = Promise.resolve(this._currentEventPromise);
    return this._destroyedPromise;
  },

  observe(subj, topic, data) {
    switch (topic) {
      case "browser-search-service":
        if (data != "init-complete") {
          break;
        }
      // fall through
      case "nsPref:changed":
      case "browser-search-engine-modified":
        this._eventQueue.push({
          type: "Observe",
          data,
        });
        this._processEventQueue();
        break;
      case "shutdown-leaks-before-check":
        subj.wrappedJSObject.client.addBlocker(
          "ContentSearch: Wait until the service is destroyed",
          () => this.destroy()
        );
        break;
    }
  },

  /**
   * Observes changes in prefs tracked by UrlbarPrefs.
   * @param {string} pref
   *   The name of the pref, relative to `browser.urlbar.` if the pref is
   *   in that branch.
   */
  onPrefChanged(pref) {
    if (lazy.UrlbarPrefs.shouldHandOffToSearchModePrefs.includes(pref)) {
      this._eventQueue.push({
        type: "Observe",
        data: "shouldHandOffToSearchMode",
      });
      this._processEventQueue();
    }
  },

  removeFormHistoryEntry(browser, entry) {
    let browserData = this._suggestionDataForBrowser(browser);
    if (browserData && browserData.previousFormHistoryResult) {
      let { previousFormHistoryResult } = browserData;
      for (let i = 0; i < previousFormHistoryResult.matchCount; i++) {
        if (previousFormHistoryResult.getValueAt(i) === entry) {
          previousFormHistoryResult.removeValueAt(i);
          break;
        }
      }
    }
  },

  performSearch(actor, browser, data) {
    this._ensureDataHasProperties(data, [
      "engineName",
      "searchString",
      "healthReportKey",
      "searchPurpose",
    ]);
    let engine = Services.search.getEngineByName(data.engineName);
    let submission = engine.getSubmission(
      data.searchString,
      "",
      data.searchPurpose
    );
    let win = browser.ownerGlobal;
    if (!win) {
      // The browser may have been closed between the time its content sent the
      // message and the time we handle it.
      return;
    }
    let where = win.whereToOpenLink(data.originalEvent);

    // There is a chance that by the time we receive the search message, the user
    // has switched away from the tab that triggered the search. If, based on the
    // event, we need to load the search in the same tab that triggered it (i.e.
    // where === "current"), openUILinkIn will not work because that tab is no
    // longer the current one. For this case we manually load the URI.
    if (where === "current") {
      // Since we're going to load the search in the same browser, blur the search
      // UI to prevent further interaction before we start loading.
      this._reply(actor, "Blur");
      browser.loadURI(submission.uri.spec, {
        postData: submission.postData,
        triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
          {
            userContextId: win.gBrowser.selectedBrowser.getAttribute(
              "userContextId"
            ),
          }
        ),
      });
    } else {
      let params = {
        postData: submission.postData,
        inBackground: Services.prefs.getBoolPref(
          "browser.tabs.loadInBackground"
        ),
      };
      win.openTrustedLinkIn(submission.uri.spec, where, params);
    }
    lazy.BrowserSearchTelemetry.recordSearch(
      browser,
      engine,
      data.healthReportKey,
      {
        selection: data.selection,
        url: submission.uri,
      }
    );
  },

  async getSuggestions(engineName, searchString, browser) {
    let engine = Services.search.getEngineByName(engineName);
    if (!engine) {
      throw new Error("Unknown engine name: " + engineName);
    }

    let browserData = this._suggestionDataForBrowser(browser, true);
    let { controller } = browserData;
    let ok = lazy.SearchSuggestionController.engineOffersSuggestions(engine);
    controller.maxLocalResults = ok ? MAX_LOCAL_SUGGESTIONS : MAX_SUGGESTIONS;
    controller.maxRemoteResults = ok ? MAX_SUGGESTIONS : 0;
    let priv = lazy.PrivateBrowsingUtils.isBrowserPrivate(browser);
    // fetch() rejects its promise if there's a pending request, but since we
    // process our event queue serially, there's never a pending request.
    this._currentSuggestion = { controller, browser };
    let suggestions = await controller.fetch(searchString, priv, engine);

    // Simplify results since we do not support rich results in this component.
    suggestions.local = suggestions.local.map(e => e.value);
    // We shouldn't show tail suggestions in their full-text form.
    let nonTailEntries = suggestions.remote.filter(
      e => !e.matchPrefix && !e.tail
    );
    suggestions.remote = nonTailEntries.map(e => e.value);

    this._currentSuggestion = null;

    // suggestions will be null if the request was cancelled
    let result = {};
    if (!suggestions) {
      return result;
    }

    // Keep the form history result so RemoveFormHistoryEntry can remove entries
    // from it.  Keeping only one result isn't foolproof because the client may
    // try to remove an entry from one set of suggestions after it has requested
    // more but before it's received them.  In that case, the entry may not
    // appear in the new suggestions.  But that should happen rarely.
    browserData.previousFormHistoryResult = suggestions.formHistoryResult;
    result = {
      engineName,
      term: suggestions.term,
      local: suggestions.local,
      remote: suggestions.remote,
    };
    return result;
  },

  async addFormHistoryEntry(browser, entry = null) {
    let isPrivate = false;
    try {
      // isBrowserPrivate assumes that the passed-in browser has all the normal
      // properties, which won't be true if the browser has been destroyed.
      // That may be the case here due to the asynchronous nature of messaging.
      isPrivate = lazy.PrivateBrowsingUtils.isBrowserPrivate(browser);
    } catch (err) {
      return false;
    }
    if (
      isPrivate ||
      !entry ||
      entry.value.length >
        lazy.SearchSuggestionController.SEARCH_HISTORY_MAX_VALUE_LENGTH
    ) {
      return false;
    }
    let browserData = this._suggestionDataForBrowser(browser, true);
    lazy.FormHistory.update(
      {
        op: "bump",
        fieldname: browserData.controller.formHistoryParam,
        value: entry.value,
        source: entry.engineName,
      },
      {
        handleCompletion: () => {},
        handleError: err => {
          Cu.reportError("Error adding form history entry: " + err);
        },
      }
    );
    return true;
  },

  async currentStateObj(window) {
    let state = {
      engines: [],
      currentEngine: await this._currentEngineObj(false),
      currentPrivateEngine: await this._currentEngineObj(true),
    };

    let pref = Services.prefs.getStringPref("browser.search.hiddenOneOffs");
    let hiddenList = pref ? pref.split(",") : [];
    for (let engine of await Services.search.getVisibleEngines()) {
      state.engines.push({
        name: engine.name,
        iconData: await this._getEngineIconURL(engine),
        hidden: hiddenList.includes(engine.name),
        isAppProvided: engine.isAppProvided,
      });
    }

    if (window) {
      state.isInPrivateBrowsingMode = lazy.PrivateBrowsingUtils.isContentWindowPrivate(
        window
      );
      state.isAboutPrivateBrowsing =
        window.gBrowser.currentURI.spec == "about:privatebrowsing";
    }

    return state;
  },

  _processEventQueue() {
    if (this._currentEventPromise || !this._eventQueue.length) {
      return;
    }

    let event = this._eventQueue.shift();

    this._currentEventPromise = (async () => {
      try {
        await this["_on" + event.type](event);
      } catch (err) {
        Cu.reportError(err);
      } finally {
        this._currentEventPromise = null;

        this._processEventQueue();
      }
    })();
  },

  _cancelSuggestions({ actor, browser }) {
    let cancelled = false;
    // cancel active suggestion request
    if (
      this._currentSuggestion &&
      this._currentSuggestion.browser === browser
    ) {
      this._currentSuggestion.controller.stop();
      cancelled = true;
    }
    // cancel queued suggestion requests
    for (let i = 0; i < this._eventQueue.length; i++) {
      let m = this._eventQueue[i];
      if (actor === m.actor && m.name === "GetSuggestions") {
        this._eventQueue.splice(i, 1);
        cancelled = true;
        i--;
      }
    }
    if (cancelled) {
      this._reply(actor, "SuggestionsCancelled");
    }
  },

  async _onMessage(eventItem) {
    let methodName = "_onMessage" + eventItem.name;
    if (methodName in this) {
      await this._initService();
      await this[methodName](eventItem);
      eventItem.browser.removeEventListener("SwapDocShells", eventItem, true);
    }
  },

  _onMessageGetState({ actor, browser }) {
    return this.currentStateObj(browser.ownerGlobal).then(state => {
      this._reply(actor, "State", state);
    });
  },

  _onMessageGetEngine({ actor, browser }) {
    return this.currentStateObj(browser.ownerGlobal).then(state => {
      this._reply(actor, "Engine", {
        isPrivateEngine: state.isInPrivateBrowsingMode,
        isAboutPrivateBrowsing: state.isAboutPrivateBrowsing,
        engine: state.isInPrivateBrowsingMode
          ? state.currentPrivateEngine
          : state.currentEngine,
      });
    });
  },

  _onMessageGetHandoffSearchModePrefs({ actor }) {
    this._reply(
      actor,
      "HandoffSearchModePrefs",
      lazy.UrlbarPrefs.get("shouldHandOffToSearchMode")
    );
  },

  _onMessageGetStrings({ actor }) {
    this._reply(actor, "Strings", this.searchSuggestionUIStrings);
  },

  _onMessageSearch({ actor, browser, data }) {
    this.performSearch(actor, browser, data);
  },

  _onMessageSetCurrentEngine({ data }) {
    Services.search.defaultEngine = Services.search.getEngineByName(data);
  },

  _onMessageManageEngines({ browser }) {
    browser.ownerGlobal.openPreferences("paneSearch");
  },

  async _onMessageGetSuggestions({ actor, browser, data }) {
    this._ensureDataHasProperties(data, ["engineName", "searchString"]);
    let { engineName, searchString } = data;
    let suggestions = await this.getSuggestions(
      engineName,
      searchString,
      browser
    );

    this._reply(actor, "Suggestions", {
      engineName: data.engineName,
      searchString: suggestions.term,
      formHistory: suggestions.local,
      remote: suggestions.remote,
    });
  },

  async _onMessageAddFormHistoryEntry({ browser, data: entry }) {
    await this.addFormHistoryEntry(browser, entry);
  },

  _onMessageRemoveFormHistoryEntry({ browser, data: entry }) {
    this.removeFormHistoryEntry(browser, entry);
  },

  _onMessageSpeculativeConnect({ browser, data: engineName }) {
    let engine = Services.search.getEngineByName(engineName);
    if (!engine) {
      throw new Error("Unknown engine name: " + engineName);
    }
    if (browser.contentWindow) {
      engine.speculativeConnect({
        window: browser.contentWindow,
        originAttributes: browser.contentPrincipal.originAttributes,
      });
    }
  },

  async _onObserve(eventItem) {
    let engine;
    switch (eventItem.data) {
      case "engine-default":
        engine = await this._currentEngineObj(false);
        this._broadcast("CurrentEngine", engine);
        break;
      case "engine-default-private":
        engine = await this._currentEngineObj(true);
        this._broadcast("CurrentPrivateEngine", engine);
        break;
      case "shouldHandOffToSearchMode":
        this._broadcast(
          "HandoffSearchModePrefs",
          lazy.UrlbarPrefs.get("shouldHandOffToSearchMode")
        );
        break;
      default:
        let state = await this.currentStateObj();
        this._broadcast("CurrentState", state);
        break;
    }
  },

  _suggestionDataForBrowser(browser, create = false) {
    let data = this._suggestionMap.get(browser);
    if (!data && create) {
      // Since one SearchSuggestionController instance is meant to be used per
      // autocomplete widget, this means that we assume each xul:browser has at
      // most one such widget.
      data = {
        controller: new lazy.SearchSuggestionController(),
      };
      this._suggestionMap.set(browser, data);
    }
    return data;
  },

  _reply(actor, type, data) {
    actor.sendAsyncMessage(type, data);
  },

  _broadcast(type, data) {
    for (let actor of gContentSearchActors) {
      actor.sendAsyncMessage(type, data);
    }
  },

  async _currentEngineObj(usePrivate) {
    let engine =
      Services.search[usePrivate ? "defaultPrivateEngine" : "defaultEngine"];
    let obj = {
      name: engine.name,
      iconData: await this._getEngineIconURL(engine),
      isAppProvided: engine.isAppProvided,
    };
    return obj;
  },

  /**
   * Converts the engine's icon into an appropriate URL for display at
   */
  async _getEngineIconURL(engine) {
    let url = engine.getIconURLBySize(16, 16);
    if (!url) {
      return SEARCH_ENGINE_PLACEHOLDER_ICON;
    }

    // The uri received here can be of two types
    // 1 - moz-extension://[uuid]/path/to/icon.ico
    // 2 - data:image/x-icon;base64,VERY-LONG-STRING
    //
    // If the URI is not a data: URI, there's no point in converting
    // it to an arraybuffer (which is used to optimize passing the data
    // accross processes): we can just pass the original URI, which is cheaper.
    if (!url.startsWith("data:")) {
      return url;
    }

    return new Promise(resolve => {
      let xhr = new XMLHttpRequest();
      xhr.open("GET", url, true);
      xhr.responseType = "arraybuffer";
      xhr.onload = () => {
        resolve(xhr.response);
      };
      xhr.onerror = xhr.onabort = xhr.ontimeout = () => {
        resolve(SEARCH_ENGINE_PLACEHOLDER_ICON);
      };
      try {
        // This throws if the URI is erroneously encoded.
        xhr.send();
      } catch (err) {
        resolve(SEARCH_ENGINE_PLACEHOLDER_ICON);
      }
    });
  },

  _ensureDataHasProperties(data, requiredProperties) {
    for (let prop of requiredProperties) {
      if (!(prop in data)) {
        throw new Error("Message data missing required property: " + prop);
      }
    }
  },

  _initService() {
    if (!this._initServicePromise) {
      this._initServicePromise = Services.search.init();
    }
    return this._initServicePromise;
  },
};

class ContentSearchParent extends JSWindowActorParent {
  constructor() {
    super();
    ContentSearch.init();
    gContentSearchActors.add(this);
  }

  didDestroy() {
    gContentSearchActors.delete(this);
  }

  receiveMessage(msg) {
    // Add a temporary event handler that exists only while the message is in
    // the event queue.  If the message's source docshell changes browsers in
    // the meantime, then we need to update the browser.  event.detail will be
    // the docshell's new parent <xul:browser> element.
    let browser = this.browsingContext.top.embedderElement;
    let eventItem = {
      type: "Message",
      name: msg.name,
      data: msg.data,
      browser,
      actor: this,
      handleEvent: event => {
        let browserData = ContentSearch._suggestionMap.get(eventItem.browser);
        if (browserData) {
          ContentSearch._suggestionMap.delete(eventItem.browser);
          ContentSearch._suggestionMap.set(event.detail, browserData);
        }
        browser.removeEventListener("SwapDocShells", eventItem, true);
        eventItem.browser = event.detail;
        eventItem.browser.addEventListener("SwapDocShells", eventItem, true);
      },
    };
    browser.addEventListener("SwapDocShells", eventItem, true);

    // Search requests cause cancellation of all Suggestion requests from the
    // same browser.
    if (msg.name === "Search") {
      ContentSearch._cancelSuggestions(eventItem);
    }

    ContentSearch._eventQueue.push(eventItem);
    ContentSearch._processEventQueue();
  }
}
