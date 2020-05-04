/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ContentSearchParent", "ContentSearch"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["XMLHttpRequest"]);

ChromeUtils.defineModuleGetter(
  this,
  "FormHistory",
  "resource://gre/modules/FormHistory.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "SearchSuggestionController",
  "resource://gre/modules/SearchSuggestionController.jsm"
);

const MAX_LOCAL_SUGGESTIONS = 3;
const MAX_SUGGESTIONS = 6;

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
      this._stringBundle = Services.strings.createBundle(
        "chrome://global/locale/autocomplete.properties"
      );

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

  performSearch(browser, data) {
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
      this._reply(browser, "Blur");
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
    win.BrowserSearch.recordSearchInTelemetry(engine, data.healthReportKey, {
      selection: data.selection,
    });
  },

  async getSuggestions(engineName, searchString, browser) {
    let engine = Services.search.getEngineByName(engineName);
    if (!engine) {
      throw new Error("Unknown engine name: " + engineName);
    }

    let browserData = this._suggestionDataForBrowser(browser, true);
    let { controller } = browserData;
    let ok = SearchSuggestionController.engineOffersSuggestions(engine);
    controller.maxLocalResults = ok ? MAX_LOCAL_SUGGESTIONS : MAX_SUGGESTIONS;
    controller.maxRemoteResults = ok ? MAX_SUGGESTIONS : 0;
    let priv = PrivateBrowsingUtils.isBrowserPrivate(browser);
    // fetch() rejects its promise if there's a pending request, but since we
    // process our event queue serially, there's never a pending request.
    this._currentSuggestion = { controller, browser };
    let suggestions = await controller.fetch(searchString, priv, engine);
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

  async addFormHistoryEntry(browser, entry = "") {
    let isPrivate = false;
    try {
      // isBrowserPrivate assumes that the passed-in browser has all the normal
      // properties, which won't be true if the browser has been destroyed.
      // That may be the case here due to the asynchronous nature of messaging.
      isPrivate = PrivateBrowsingUtils.isBrowserPrivate(browser);
    } catch (err) {
      return false;
    }
    if (isPrivate || entry === "") {
      return false;
    }
    let browserData = this._suggestionDataForBrowser(browser, true);
    FormHistory.update(
      {
        op: "bump",
        fieldname: browserData.controller.formHistoryParam,
        value: entry,
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
      let uri = engine.getIconURLBySize(16, 16);
      let iconData = await this._maybeConvertURIToArrayBuffer(uri);

      state.engines.push({
        name: engine.name,
        iconData,
        hidden: hiddenList.includes(engine.name),
        isAppProvided: engine.isAppProvided,
      });
    }

    if (window) {
      state.isPrivateWindow = PrivateBrowsingUtils.isContentWindowPrivate(
        window
      );
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

  _cancelSuggestions(browser) {
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
      if (browser === m.browser && m.name === "GetSuggestions") {
        this._eventQueue.splice(i, 1);
        cancelled = true;
        i--;
      }
    }
    if (cancelled) {
      this._reply(browser, "SuggestionsCancelled");
    }
  },

  async _onMessage(eventItem) {
    let methodName = "_onMessage" + eventItem.name;
    if (methodName in this) {
      await this._initService();
      await this[methodName](eventItem.browser, eventItem.data);
      eventItem.browser.removeEventListener("SwapDocShells", eventItem, true);
    }
  },

  _onMessageGetState(browser, data) {
    return this.currentStateObj(browser.ownerGlobal).then(state => {
      this._reply(browser, "State", state);
    });
  },

  _onMessageGetEngine(browser, data) {
    return this.currentStateObj(browser.ownerGlobal).then(state => {
      this._reply(browser, "Engine", {
        isPrivateWindow: state.isPrivateWindow,
        engine: state.isPrivateWindow
          ? state.currentPrivateEngine
          : state.currentEngine,
      });
    });
  },

  _onMessageGetStrings(browser, data) {
    this._reply(browser, "Strings", this.searchSuggestionUIStrings);
  },

  _onMessageSearch(browser, data) {
    this.performSearch(browser, data);
  },

  _onMessageSetCurrentEngine(browser, data) {
    Services.search.defaultEngine = Services.search.getEngineByName(data);
  },

  _onMessageManageEngines(browser) {
    browser.ownerGlobal.openPreferences("paneSearch");
  },

  async _onMessageGetSuggestions(browser, data) {
    this._ensureDataHasProperties(data, ["engineName", "searchString"]);
    let { engineName, searchString } = data;
    let suggestions = await this.getSuggestions(
      engineName,
      searchString,
      browser
    );

    this._reply(browser, "Suggestions", {
      engineName: data.engineName,
      searchString: suggestions.term,
      formHistory: suggestions.local,
      remote: suggestions.remote,
    });
  },

  async _onMessageAddFormHistoryEntry(browser, entry) {
    await this.addFormHistoryEntry(browser, entry);
  },

  _onMessageRemoveFormHistoryEntry(browser, entry) {
    this.removeFormHistoryEntry(browser, entry);
  },

  _onMessageSpeculativeConnect(browser, engineName) {
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
    if (eventItem.data === "engine-default") {
      let engine = await this._currentEngineObj(false);
      this._broadcast("CurrentEngine", engine);
    } else if (eventItem.data === "engine-default-private") {
      let engine = await this._currentEngineObj(true);
      this._broadcast("CurrentPrivateEngine", engine);
    } else {
      let state = await this.currentStateObj();
      this._broadcast("CurrentState", state);
    }
  },

  _suggestionDataForBrowser(browser, create = false) {
    let data = this._suggestionMap.get(browser);
    if (!data && create) {
      // Since one SearchSuggestionController instance is meant to be used per
      // autocomplete widget, this means that we assume each xul:browser has at
      // most one such widget.
      data = {
        controller: new SearchSuggestionController(),
      };
      this._suggestionMap.set(browser, data);
    }
    return data;
  },

  _reply(browser, type, data) {
    browser.sendMessageToActor(type, data, "ContentSearch");
  },

  _broadcast(type, data) {
    for (let actor of gContentSearchActors) {
      actor.sendAsyncMessage(type, data);
    }
  },

  async _currentEngineObj(usePrivate) {
    let engine =
      Services.search[usePrivate ? "defaultPrivateEngine" : "defaultEngine"];
    let favicon = engine.getIconURLBySize(16, 16);
    let placeholder = this._stringBundle.formatStringFromName(
      "searchWithEngine",
      [engine.name]
    );
    let obj = {
      name: engine.name,
      placeholder,
      iconData: await this._maybeConvertURIToArrayBuffer(favicon),
      isAppProvided: engine.isAppProvided,
    };
    return obj;
  },

  _maybeConvertURIToArrayBuffer(uri) {
    if (!uri) {
      return Promise.resolve(null);
    }

    // The uri received here can be of two types
    // 1 - moz-extension://[uuid]/path/to/icon.ico
    // 2 - data:image/x-icon;base64,VERY-LONG-STRING
    //
    // If the URI is not a data: URI, there's no point in converting
    // it to an arraybuffer (which is used to optimize passing the data
    // accross processes): we can just pass the original URI, which is cheaper.
    if (!uri.startsWith("data:")) {
      return Promise.resolve(uri);
    }

    return new Promise(resolve => {
      let xhr = new XMLHttpRequest();
      xhr.open("GET", uri, true);
      xhr.responseType = "arraybuffer";
      xhr.onload = () => {
        resolve(xhr.response);
      };
      xhr.onerror = xhr.onabort = xhr.ontimeout = () => {
        resolve(null);
      };
      try {
        // This throws if the URI is erroneously encoded.
        xhr.send();
      } catch (err) {
        resolve(null);
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
      ContentSearch._cancelSuggestions();
    }

    ContentSearch._eventQueue.push(eventItem);
    ContentSearch._processEventQueue();
  }
}
