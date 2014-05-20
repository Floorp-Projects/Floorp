/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "ContentSearch",
];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

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
 *   State
 *     Sent in reply to GetState and when the state changes.
 *     data: see _currentStateObj
 */

this.ContentSearch = {

  init: function () {
    Cc["@mozilla.org/globalmessagemanager;1"].
      getService(Ci.nsIMessageListenerManager).
      addMessageListener(INBOUND_MESSAGE, this);
    Services.obs.addObserver(this, "browser-search-engine-modified", false);
  },

  receiveMessage: function (msg) {
    let methodName = "on" + msg.data.type;
    if (methodName in this) {
      this._initService().then(() => {
        this[methodName](msg, msg.data.data);
      });
    }
  },

  onGetState: function (msg, data) {
    this._reply(msg, "State", this._currentStateObj());
  },

  onSearch: function (msg, data) {
    let expectedDataProps = [
      "engineName",
      "searchString",
      "whence",
    ];
    for (let prop of expectedDataProps) {
      if (!(prop in data)) {
        Cu.reportError("Message data missing required property: " + prop);
        return;
      }
    }
    let browserWin = msg.target.ownerDocument.defaultView;
    let engine = Services.search.getEngineByName(data.engineName);
    browserWin.BrowserSearch.recordSearchInHealthReport(engine, data.whence);
    let submission = engine.getSubmission(data.searchString, "", data.whence);
    browserWin.loadURI(submission.uri.spec, null, submission.postData);
  },

  onSetCurrentEngine: function (msg, data) {
    Services.search.currentEngine = Services.search.getEngineByName(data);
  },

  onManageEngines: function (msg, data) {
    let browserWin = msg.target.ownerDocument.defaultView;
    browserWin.BrowserSearch.searchBar.openManager(null);
  },

  observe: function (subj, topic, data) {
    this._initService().then(() => {
      switch (topic) {
      case "browser-search-engine-modified":
        if (data == "engine-current") {
          this._broadcast("CurrentEngine", this._currentEngineObj());
        }
        else if (data != "engine-default") {
          // engine-default is always sent with engine-current and isn't
          // otherwise relevant to content searches.
          this._broadcast("State", this._currentStateObj());
        }
        break;
      }
    });
  },

  _reply: function (msg, type, data) {
    msg.target.messageManager.sendAsyncMessage(...this._msgArgs(type, data));
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

  _currentStateObj: function () {
    return {
      engines: Services.search.getVisibleEngines().map(engine => {
        return {
          name: engine.name,
          iconURI: engine.getIconURLBySize(16, 16),
        };
      }),
      currentEngine: this._currentEngineObj(),
    };
  },

  _currentEngineObj: function () {
    return {
      name: Services.search.currentEngine.name,
      logoURI: Services.search.currentEngine.getIconURLBySize(65, 26),
      logo2xURI: Services.search.currentEngine.getIconURLBySize(130, 52),
    };
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
