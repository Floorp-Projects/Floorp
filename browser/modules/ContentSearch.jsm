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

const INBOUND_MESSAGE = "ContentSearch";
const OUTBOUND_MESSAGE = INBOUND_MESSAGE;

/**
 * ContentSearch receives messages named INBOUND_MESSAGE and sends messages
 * named OUTBOUND_MESSAGE.  The data of each message is expected to look like
 * { type, data }.  type is the message's type (or subtype if you consider the
 * type of the message itself to be INBOUND_MESSAGE), and data is data that is
 * specific to the type.
 *
 * Inbound messages have the following types:
 *
 *   GetState
 *      Retrieves the current search engine state.
 *      data: null
 *   ManageEngines
 *      Opens the search engine management window.
 *      data: null
 *   Search
 *      Performs a search.
 *      data: an object { engineName, searchString, whence }
 *   SetCurrentEngine
 *      Sets the current engine.
 *      data: the name of the engine
 *
 * Outbound messages have the following types:
 *
 *   CurrentEngine
 *     Sent when the current engine changes.
 *     data: see _currentEngineObj
 *   CurrentState
 *     Sent when the current search state changes.
 *     data: see _currentStateObj
 *   State
 *     Sent in reply to GetState.
 *     data: see _currentStateObj
 */

this.ContentSearch = {

  // Inbound events are queued and processed in FIFO order instead of handling
  // them immediately, which would result in non-FIFO responses due to the
  // asynchrononicity added by converting image data URIs to ArrayBuffers.
  _eventQueue: [],
  _currentEvent: null,

  init: function () {
    Cc["@mozilla.org/globalmessagemanager;1"].
      getService(Ci.nsIMessageListenerManager).
      addMessageListener(INBOUND_MESSAGE, this);
    Services.obs.addObserver(this, "browser-search-engine-modified", false);
  },

  receiveMessage: function (msg) {
    // Add a temporary event handler that exists only while the message is in
    // the event queue.  If the message's source docshell changes browsers in
    // the meantime, then we need to update msg.target.  event.detail will be
    // the docshell's new parent <xul:browser> element.
    msg.handleEvent = function (event) {
      this.target.removeEventListener("SwapDocShells", this, true);
      this.target = event.detail;
      this.target.addEventListener("SwapDocShells", this, true);
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
    }
  },

  _processEventQueue: Task.async(function* () {
    if (this._currentEvent || !this._eventQueue.length) {
      return;
    }
    this._currentEvent = this._eventQueue.shift();
    try {
      yield this["_on" + this._currentEvent.type](this._currentEvent.data);
    }
    finally {
      this._currentEvent = null;
      this._processEventQueue();
    }
  }),

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
    let expectedDataProps = [
      "engineName",
      "searchString",
      "whence",
    ];
    for (let prop of expectedDataProps) {
      if (!(prop in data)) {
        Cu.reportError("Message data missing required property: " + prop);
        return Promise.resolve();
      }
    }
    let browserWin = msg.target.ownerDocument.defaultView;
    let engine = Services.search.getEngineByName(data.engineName);
    browserWin.BrowserSearch.recordSearchInHealthReport(engine, data.whence);
    let submission = engine.getSubmission(data.searchString, "", data.whence);
    browserWin.loadURI(submission.uri.spec, null, submission.postData);
    return Promise.resolve();
  },

  _onMessageSetCurrentEngine: function (msg, data) {
    Services.search.currentEngine = Services.search.getEngineByName(data);
    return Promise.resolve();
  },

  _onMessageManageEngines: function (msg, data) {
    let browserWin = msg.target.ownerDocument.defaultView;
    browserWin.BrowserSearch.searchBar.openManager(null);
    return Promise.resolve();
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
    let uri1x = engine.getIconURLBySize(65, 26);
    let uri2x = engine.getIconURLBySize(130, 52);
    let obj = {
      name: engine.name,
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

  _initService: function () {
    if (!this._initServicePromise) {
      let deferred = Promise.defer();
      this._initServicePromise = deferred.promise;
      Services.search.init(() => deferred.resolve());
    }
    return this._initServicePromise;
  },
};
