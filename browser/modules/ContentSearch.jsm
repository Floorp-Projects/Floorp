/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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
 *     data: { engineName, searchString, [remoteTimeout] }
 *   GetState
 *     Retrieves the current search engine state.
 *     data: null
 *   ManageEngines
 *     Opens the search engine management window.
 *     data: null
 *   RemoveFormHistoryEntry
 *     Removes an entry from the search form history.
 *     data: the entry, a string
 *   Search
 *     Performs a search.
 *     data: { engineName, searchString, whence }
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
 *     data: see _currentStateObj
 *   State
 *     Sent in reply to GetState.
 *     data: see _currentStateObj
 *   Suggestions
 *     Sent in reply to GetSuggestions.
 *     data: see _onMessageGetSuggestions
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

  init: function () {
    Cc["@mozilla.org/globalmessagemanager;1"].
      getService(Ci.nsIMessageListenerManager).
      addMessageListener(INBOUND_MESSAGE, this);
    Services.obs.addObserver(this, "browser-search-engine-modified", false);
    Services.obs.addObserver(this, "shutdown-leaks-before-check", false);
    this._stringBundle = Services.strings.createBundle("chrome://global/locale/autocomplete.properties");
  },

  destroy: function () {
    if (this._destroyedPromise) {
      return this._destroyedPromise;
    }

    Cc["@mozilla.org/globalmessagemanager;1"].
      getService(Ci.nsIMessageListenerManager).
      removeMessageListener(INBOUND_MESSAGE, this);
    Services.obs.removeObserver(this, "browser-search-engine-modified");
    Services.obs.removeObserver(this, "shutdown-leaks-before-check");

    this._eventQueue.length = 0;
    return this._destroyedPromise = Promise.resolve(this._currentEventPromise);
  },

  /**
   * Focuses the search input in the page with the given message manager.
   * @param  messageManager
   *         The MessageManager object of the selected browser.
   */
  focusInput: function (messageManager) {
    messageManager.sendAsyncMessage(OUTBOUND_MESSAGE, {
      type: "FocusInput"
    });
  },

  receiveMessage: function (msg) {
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

    this._eventQueue.push({
      type: "Message",
      data: msg,
    });
    this._processEventQueue();
  },

  observe: function (subj, topic, data) {
    switch (topic) {
    case "browser-search-engine-modified":
      this._eventQueue.push({
        type: "Observe",
        data: data,
      });
      this._processEventQueue();
      break;
    case "shutdown-leaks-before-check":
      subj.wrappedJSObject.client.addBlocker(
        "ContentSearch: Wait until the service is destroyed", () => this.destroy());
      break;
    }
  },

  _processEventQueue: function () {
    if (this._currentEventPromise || !this._eventQueue.length) {
      return;
    }

    let event = this._eventQueue.shift();

    return this._currentEventPromise = Task.spawn(function* () {
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

  _onMessage: Task.async(function* (msg) {
    let methodName = "_onMessage" + msg.data.type;
    if (methodName in this) {
      yield this._initService();
      yield this[methodName](msg, msg.data.data);
      msg.target.removeEventListener("SwapDocShells", msg, true);
    }
  }),

  _onMessageGetState: function (msg, data) {
    return this._currentStateObj().then(state => {
      this._reply(msg, "State", state);
    });
  },

  _onMessageSearch: function (msg, data) {
    this._ensureDataHasProperties(data, [
      "engineName",
      "searchString",
      "whence",
    ]);
    let engine = Services.search.getEngineByName(data.engineName);
    let submission = engine.getSubmission(data.searchString, "", data.whence);
    let browser = msg.target;
    let win;
    try {
      win = browser.ownerDocument.defaultView;
    }
    catch (err) {
      // The browser may have been closed between the time its content sent the
      // message and the time we handle it.  In that case, trying to call any
      // method on it will throw.
      return Promise.resolve();
    }

    let where = win.whereToOpenLink(data.originalEvent);

    // There is a chance that by the time we receive the search message, the user
    // has switched away from the tab that triggered the search. If, based on the
    // event, we need to load the search in the same tab that triggered it (i.e.
    // where == "current"), openUILinkIn will not work because that tab is no
    // longer the current one. For this case we manually load the URI.
    if (where == "current") {
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
    win.BrowserSearch.recordSearchInHealthReport(engine, data.whence,
                                                 data.selection || null);
    return Promise.resolve();
  },

  _onMessageSetCurrentEngine: function (msg, data) {
    Services.search.currentEngine = Services.search.getEngineByName(data);
    return Promise.resolve();
  },

  _onMessageManageEngines: function (msg, data) {
    let browserWin = msg.target.ownerDocument.defaultView;

    if (Services.prefs.getBoolPref("browser.search.showOneOffButtons")) {
      browserWin.openPreferences("paneSearch");
      return Promise.resolve();
    }

    let wm = Components.classes["@mozilla.org/appshell/window-mediator;1"].
             getService(Components.interfaces.nsIWindowMediator);
    let window = wm.getMostRecentWindow("Browser:SearchManager");

    if (window) {
      window.focus()
    }
    else {
      browserWin.setTimeout(function () {
        browserWin.openDialog("chrome://browser/content/search/engineManager.xul",
          "_blank", "chrome,dialog,modal,centerscreen,resizable");
      }, 0);
    }
    return Promise.resolve();
  },

  _onMessageGetSuggestions: Task.async(function* (msg, data) {
    this._ensureDataHasProperties(data, [
      "engineName",
      "searchString",
    ]);

    let engine = Services.search.getEngineByName(data.engineName);
    if (!engine) {
      throw new Error("Unknown engine name: " + data.engineName);
    }

    let browserData = this._suggestionDataForBrowser(msg.target, true);
    let { controller } = browserData;
    let ok = SearchSuggestionController.engineOffersSuggestions(engine);
    controller.maxLocalResults = ok ? MAX_LOCAL_SUGGESTIONS : MAX_SUGGESTIONS;
    controller.maxRemoteResults = ok ? MAX_SUGGESTIONS : 0;
    controller.remoteTimeout = data.remoteTimeout || undefined;
    let priv = PrivateBrowsingUtils.isBrowserPrivate(msg.target);
    // fetch() rejects its promise if there's a pending request, but since we
    // process our event queue serially, there's never a pending request.
    let suggestions = yield controller.fetch(data.searchString, priv, engine);

    // Keep the form history result so RemoveFormHistoryEntry can remove entries
    // from it.  Keeping only one result isn't foolproof because the client may
    // try to remove an entry from one set of suggestions after it has requested
    // more but before it's received them.  In that case, the entry may not
    // appear in the new suggestions.  But that should happen rarely.
    browserData.previousFormHistoryResult = suggestions.formHistoryResult;

    this._reply(msg, "Suggestions", {
      engineName: data.engineName,
      searchString: suggestions.term,
      formHistory: suggestions.local,
      remote: suggestions.remote,
    });
  }),

  _onMessageAddFormHistoryEntry: function (msg, entry) {
    let isPrivate = true;
    try {
      // isBrowserPrivate assumes that the passed-in browser has all the normal
      // properties, which won't be true if the browser has been destroyed.
      // That may be the case here due to the asynchronous nature of messaging.
      isPrivate = PrivateBrowsingUtils.isBrowserPrivate(msg.target);
    } catch (err) {}
    if (isPrivate || entry === "") {
      return Promise.resolve();
    }
    let browserData = this._suggestionDataForBrowser(msg.target, true);
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
    return Promise.resolve();
  },

  _onMessageRemoveFormHistoryEntry: function (msg, entry) {
    let browserData = this._suggestionDataForBrowser(msg.target);
    if (browserData && browserData.previousFormHistoryResult) {
      let { previousFormHistoryResult } = browserData;
      for (let i = 0; i < previousFormHistoryResult.matchCount; i++) {
        if (previousFormHistoryResult.getValueAt(i) == entry) {
          previousFormHistoryResult.removeValueAt(i, true);
          break;
        }
      }
    }
    return Promise.resolve();
  },

  _onMessageSpeculativeConnect: function (msg, engineName) {
    let engine = Services.search.getEngineByName(engineName);
    if (!engine) {
      throw new Error("Unknown engine name: " + engineName);
    }
    if (msg.target.contentWindow) {
      engine.speculativeConnect({
        window: msg.target.contentWindow,
      });
    }
  },

  _onObserve: Task.async(function* (data) {
    if (data == "engine-current") {
      let engine = yield this._currentEngineObj();
      this._broadcast("CurrentEngine", engine);
    }
    else if (data != "engine-default") {
      // engine-default is always sent with engine-current and isn't otherwise
      // relevant to content searches.
      let state = yield this._currentStateObj();
      this._broadcast("CurrentState", state);
    }
  }),

  _suggestionDataForBrowser: function (browser, create=false) {
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

  _reply: function (msg, type, data) {
    // We reply asyncly to messages, and by the time we reply the browser we're
    // responding to may have been destroyed.  messageManager is null then.
    if (msg.target.messageManager) {
      msg.target.messageManager.sendAsyncMessage(...this._msgArgs(type, data));
    }
  },

  _broadcast: function (type, data) {
    Cc["@mozilla.org/globalmessagemanager;1"].
      getService(Ci.nsIMessageListenerManager).
      broadcastAsyncMessage(...this._msgArgs(type, data));
  },

  _msgArgs: function (type, data) {
    return [OUTBOUND_MESSAGE, {
      type: type,
      data: data,
    }];
  },

  _currentStateObj: Task.async(function* () {
    let state = {
      engines: [],
      currentEngine: yield this._currentEngineObj(),
    };
    for (let engine of Services.search.getVisibleEngines()) {
      let uri = engine.getIconURLBySize(16, 16);
      state.engines.push({
        name: engine.name,
        iconBuffer: yield this._arrayBufferFromDataURI(uri),
      });
    }
    return state;
  }),

  _currentEngineObj: Task.async(function* () {
    let engine = Services.search.currentEngine;
    let favicon = engine.getIconURLBySize(16, 16);
    let uri1x = engine.getIconURLBySize(65, 26);
    let uri2x = engine.getIconURLBySize(130, 52);
    let placeholder = this._stringBundle.formatStringFromName(
      "searchWithEngine", [engine.name], 1);
    let obj = {
      name: engine.name,
      placeholder: placeholder,
      iconBuffer: yield this._arrayBufferFromDataURI(favicon),
      logoBuffer: yield this._arrayBufferFromDataURI(uri1x),
      logo2xBuffer: yield this._arrayBufferFromDataURI(uri2x),
    };
    return obj;
  }),

  _arrayBufferFromDataURI: function (uri) {
    if (!uri) {
      return Promise.resolve(null);
    }
    let deferred = Promise.defer();
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
              createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", uri, true);
    xhr.responseType = "arraybuffer";
    xhr.onloadend = () => {
      deferred.resolve(xhr.response);
    };
    try {
      // This throws if the URI is erroneously encoded.
      xhr.send();
    }
    catch (err) {
      return Promise.resolve(null);
    }
    return deferred.promise;
  },

  _ensureDataHasProperties: function (data, requiredProperties) {
    for (let prop of requiredProperties) {
      if (!(prop in data)) {
        throw new Error("Message data missing required property: " + prop);
      }
    }
  },

  _initService: function () {
    if (!this._initServicePromise) {
      let deferred = Promise.defer();
      this._initServicePromise = deferred.promise;
      Services.search.init(() => deferred.resolve());
    }
    return this._initServicePromise;
  },
};
