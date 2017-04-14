/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals XPCOMUtils, Services, Task, Promise, SearchSuggestionController, FormHistory, PrivateBrowsingUtils */
"use strict";

this.EXPORTED_SYMBOLS = [
  "ContentSearch",
];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FormHistory",
  "resource://gre/modules/FormHistory.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SearchSuggestionController",
  "resource://gre/modules/SearchSuggestionController.jsm");

const INBOUND_MESSAGE = "ContentSearch";
const OUTBOUND_MESSAGE = INBOUND_MESSAGE;
const MAX_LOCAL_SUGGESTIONS = 3;
const MAX_SUGGESTIONS = 6;

/**
 * ContentSearch receives messages named INBOUND_MESSAGE and sends messages
 * named OUTBOUND_MESSAGE.  The data of each message is expected to look like
 * { type, data }.  type is the message's type (or subtype if you consider the
 * type of the message itself to be INBOUND_MESSAGE), and data is data that is
 * specific to the type.
 *
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

this.ContentSearch = {

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
    Cc["@mozilla.org/globalmessagemanager;1"].
      getService(Ci.nsIMessageListenerManager).
      addMessageListener(INBOUND_MESSAGE, this);
    Services.obs.addObserver(this, "browser-search-engine-modified", false);
    Services.obs.addObserver(this, "shutdown-leaks-before-check", false);
    Services.prefs.addObserver("browser.search.hiddenOneOffs", this, false);
    this._stringBundle = Services.strings.createBundle("chrome://global/locale/autocomplete.properties");
  },

  get searchSuggestionUIStrings() {
    if (this._searchSuggestionUIStrings) {
      return this._searchSuggestionUIStrings;
    }
    this._searchSuggestionUIStrings = {};
    let searchBundle = Services.strings.createBundle("chrome://browser/locale/search.properties");
    let stringNames = ["searchHeader", "searchPlaceholder", "searchForSomethingWith",
                       "searchWithHeader", "searchSettings"];

    for (let name of stringNames) {
      this._searchSuggestionUIStrings[name] = searchBundle.GetStringFromName(name);
    }
    return this._searchSuggestionUIStrings;
  },

  destroy() {
    if (this._destroyedPromise) {
      return this._destroyedPromise;
    }

    Cc["@mozilla.org/globalmessagemanager;1"].
      getService(Ci.nsIMessageListenerManager).
      removeMessageListener(INBOUND_MESSAGE, this);
    Services.obs.removeObserver(this, "browser-search-engine-modified");
    Services.obs.removeObserver(this, "shutdown-leaks-before-check");

    this._eventQueue.length = 0;
    this._destroyedPromise = Promise.resolve(this._currentEventPromise);
    return this._destroyedPromise;
  },

  /**
   * Focuses the search input in the page with the given message manager.
   * @param  messageManager
   *         The MessageManager object of the selected browser.
   */
  focusInput(messageManager) {
    messageManager.sendAsyncMessage(OUTBOUND_MESSAGE, {
      type: "FocusInput"
    });
  },

  receiveMessage(msg) {
    // Add a temporary event handler that exists only while the message is in
    // the event queue.  If the message's source docshell changes browsers in
    // the meantime, then we need to update msg.target.  event.detail will be
    // the docshell's new parent <xul:browser> element.
    msg.handleEvent = event => {
      let browserData = this._suggestionMap.get(msg.target);
      if (browserData) {
        this._suggestionMap.delete(msg.target);
        this._suggestionMap.set(event.detail, browserData);
      }
      msg.target.removeEventListener("SwapDocShells", msg, true);
      msg.target = event.detail;
      msg.target.addEventListener("SwapDocShells", msg, true);
    };
    msg.target.addEventListener("SwapDocShells", msg, true);

    // Search requests cause cancellation of all Suggestion requests from the
    // same browser.
    if (msg.data.type === "Search") {
      this._cancelSuggestions(msg);
    }

    this._eventQueue.push({
      type: "Message",
      data: msg,
    });
    this._processEventQueue();
  },

  observe(subj, topic, data) {
    switch (topic) {
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
        "ContentSearch: Wait until the service is destroyed", () => this.destroy());
      break;
    }
  },

  removeFormHistoryEntry(msg, entry) {
    let browserData = this._suggestionDataForBrowser(msg.target);
    if (browserData && browserData.previousFormHistoryResult) {
      let { previousFormHistoryResult } = browserData;
      for (let i = 0; i < previousFormHistoryResult.matchCount; i++) {
        if (previousFormHistoryResult.getValueAt(i) === entry) {
          previousFormHistoryResult.removeValueAt(i, true);
          break;
        }
      }
    }
  },

  performSearch(msg, data) {
    this._ensureDataHasProperties(data, [
      "engineName",
      "searchString",
      "healthReportKey",
      "searchPurpose",
    ]);
    let engine = Services.search.getEngineByName(data.engineName);
    let submission = engine.getSubmission(data.searchString, "", data.searchPurpose);
    let browser = msg.target;
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
    win.BrowserSearch.recordSearchInTelemetry(engine, data.healthReportKey,
                                              { selection: data.selection });
  },

  getSuggestions: Task.async(function* (engineName, searchString, browser) {
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
    this._currentSuggestion = { controller, target: browser };
    let suggestions = yield controller.fetch(searchString, priv, engine);
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
  }),

  addFormHistoryEntry: Task.async(function* (browser, entry = "") {
    let isPrivate = false;
    try {
      // isBrowserPrivate assumes that the passed-in browser has all the normal
      // properties, which won't be true if the browser has been destroyed.
      // That may be the case here due to the asynchronous nature of messaging.
      isPrivate = PrivateBrowsingUtils.isBrowserPrivate(browser.target);
    } catch (err) {
      return false;
    }
    if (isPrivate || entry === "") {
      return false;
    }
    let browserData = this._suggestionDataForBrowser(browser.target, true);
    FormHistory.update({
      op: "bump",
      fieldname: browserData.controller.formHistoryParam,
      value: entry,
    }, {
      handleCompletion: () => {},
      handleError: err => {
        Cu.reportError("Error adding form history entry: " + err);
      },
    });
    return true;
  }),

  currentStateObj: Task.async(function* (uriFlag = false) {
    let state = {
      engines: [],
      currentEngine: yield this._currentEngineObj(),
    };
    if (uriFlag) {
      state.currentEngine.iconBuffer = Services.search.currentEngine.getIconURLBySize(16, 16);
    }
    let pref = Services.prefs.getCharPref("browser.search.hiddenOneOffs");
    let hiddenList = pref ? pref.split(",") : [];
    for (let engine of Services.search.getVisibleEngines()) {
      let uri = engine.getIconURLBySize(16, 16);
      let iconBuffer = uri;
      if (!uriFlag) {
        iconBuffer = yield this._arrayBufferFromDataURI(uri);
      }
      state.engines.push({
        name: engine.name,
        iconBuffer,
        hidden: hiddenList.indexOf(engine.name) !== -1,
      });
    }
    return state;
  }),

  _processEventQueue() {
    if (this._currentEventPromise || !this._eventQueue.length) {
      return;
    }

    let event = this._eventQueue.shift();

    this._currentEventPromise = Task.spawn(function* () {
      try {
        yield this["_on" + event.type](event.data);
      } catch (err) {
        Cu.reportError(err);
      } finally {
        this._currentEventPromise = null;
        this._processEventQueue();
      }
    }.bind(this));
  },

  _cancelSuggestions(msg) {
    let cancelled = false;
    // cancel active suggestion request
    if (this._currentSuggestion && this._currentSuggestion.target === msg.target) {
      this._currentSuggestion.controller.stop();
      cancelled = true;
    }
    // cancel queued suggestion requests
    for (let i = 0; i < this._eventQueue.length; i++) {
      let m = this._eventQueue[i].data;
      if (msg.target === m.target && m.data.type === "GetSuggestions") {
        this._eventQueue.splice(i, 1);
        cancelled = true;
        i--;
      }
    }
    if (cancelled) {
      this._reply(msg, "SuggestionsCancelled");
    }
  },

  _onMessage: Task.async(function* (msg) {
    let methodName = "_onMessage" + msg.data.type;
    if (methodName in this) {
      yield this._initService();
      yield this[methodName](msg, msg.data.data);
      if (!Cu.isDeadWrapper(msg.target)) {
        msg.target.removeEventListener("SwapDocShells", msg, true);
      }
    }
  }),

  _onMessageGetState(msg, data) {
    return this.currentStateObj().then(state => {
      this._reply(msg, "State", state);
    });
  },

  _onMessageGetStrings(msg, data) {
    this._reply(msg, "Strings", this.searchSuggestionUIStrings);
  },

  _onMessageSearch(msg, data) {
    this.performSearch(msg, data);
  },

  _onMessageSetCurrentEngine(msg, data) {
    Services.search.currentEngine = Services.search.getEngineByName(data);
  },

  _onMessageManageEngines(msg, data) {
    let browserWin = msg.target.ownerGlobal;
    browserWin.openPreferences("paneGeneral");
  },

  _onMessageGetSuggestions: Task.async(function* (msg, data) {
    this._ensureDataHasProperties(data, [
      "engineName",
      "searchString",
    ]);
    let {engineName, searchString} = data;
    let suggestions = yield this.getSuggestions(engineName, searchString, msg.target);

    this._reply(msg, "Suggestions", {
      engineName: data.engineName,
      searchString: suggestions.term,
      formHistory: suggestions.local,
      remote: suggestions.remote,
    });
  }),

  _onMessageAddFormHistoryEntry: Task.async(function* (msg, entry) {
    yield this.addFormHistoryEntry(msg, entry);
  }),

  _onMessageRemoveFormHistoryEntry(msg, entry) {
    this.removeFormHistoryEntry(msg, entry);
  },

  _onMessageSpeculativeConnect(msg, engineName) {
    let engine = Services.search.getEngineByName(engineName);
    if (!engine) {
      throw new Error("Unknown engine name: " + engineName);
    }
    if (msg.target.contentWindow) {
      engine.speculativeConnect({
        window: msg.target.contentWindow,
        originAttributes: msg.target.contentPrincipal.originAttributes
      });
    }
  },

  _onObserve: Task.async(function* (data) {
    if (data === "engine-current") {
      let engine = yield this._currentEngineObj();
      this._broadcast("CurrentEngine", engine);
    } else if (data !== "engine-default") {
      // engine-default is always sent with engine-current and isn't otherwise
      // relevant to content searches.
      let state = yield this.currentStateObj();
      this._broadcast("CurrentState", state);
    }
  }),

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

  _reply(msg, type, data) {
    // We reply asyncly to messages, and by the time we reply the browser we're
    // responding to may have been destroyed.  messageManager is null then.
    if (!Cu.isDeadWrapper(msg.target) && msg.target.messageManager) {
      msg.target.messageManager.sendAsyncMessage(...this._msgArgs(type, data));
    }
  },

  _broadcast(type, data) {
    Cc["@mozilla.org/globalmessagemanager;1"].
      getService(Ci.nsIMessageListenerManager).
      broadcastAsyncMessage(...this._msgArgs(type, data));
  },

  _msgArgs(type, data) {
    return [OUTBOUND_MESSAGE, {
      type,
      data,
    }];
  },

  _currentEngineObj: Task.async(function* () {
    let engine = Services.search.currentEngine;
    let favicon = engine.getIconURLBySize(16, 16);
    let placeholder = this._stringBundle.formatStringFromName(
      "searchWithEngine", [engine.name], 1);
    let obj = {
      name: engine.name,
      placeholder,
      iconBuffer: yield this._arrayBufferFromDataURI(favicon),
    };
    return obj;
  }),

  _arrayBufferFromDataURI(uri) {
    if (!uri) {
      return Promise.resolve(null);
    }
    let deferred = Promise.defer();
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
              createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", uri, true);
    xhr.responseType = "arraybuffer";
    xhr.onload = () => {
      deferred.resolve(xhr.response);
    };
    xhr.onerror = xhr.onabort = xhr.ontimeout = () => {
      deferred.resolve(null);
    };
    try {
      // This throws if the URI is erroneously encoded.
      xhr.send();
    } catch (err) {
      return Promise.resolve(null);
    }
    return deferred.promise;
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
      let deferred = Promise.defer();
      this._initServicePromise = deferred.promise;
      Services.search.init(() => deferred.resolve());
    }
    return this._initServicePromise;
  },
};
