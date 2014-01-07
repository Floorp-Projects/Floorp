/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function debug(msg) {
  Services.console.logStringMessage("SessionStoreContent: " + msg);
}

let Cu = Components.utils;
let Cc = Components.classes;
let Ci = Components.interfaces;
let Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Timer.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "DocShellCapabilities",
  "resource:///modules/sessionstore/DocShellCapabilities.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PageStyle",
  "resource:///modules/sessionstore/PageStyle.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ScrollPosition",
  "resource:///modules/sessionstore/ScrollPosition.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionHistory",
  "resource:///modules/sessionstore/SessionHistory.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionStorage",
  "resource:///modules/sessionstore/SessionStorage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TextAndScrollData",
  "resource:///modules/sessionstore/TextAndScrollData.jsm");

Cu.import("resource:///modules/sessionstore/FrameTree.jsm", this);
let gFrameTree = new FrameTree(this);

/**
 * Returns a lazy function that will evaluate the given
 * function |fn| only once and cache its return value.
 */
function createLazy(fn) {
  let cached = false;
  let cachedValue = null;

  return function lazy() {
    if (!cached) {
      cachedValue = fn();
      cached = true;
    }

    return cachedValue;
  };
}

/**
 * Determines whether the given storage event was triggered by changes
 * to the sessionStorage object and not the local or globalStorage.
 */
function isSessionStorageEvent(event) {
  try {
    return event.storageArea == content.sessionStorage;
  } catch (ex if ex instanceof Ci.nsIException && ex.result == Cr.NS_ERROR_NOT_AVAILABLE) {
    // This page does not have a DOMSessionStorage
    // (this is typically the case for about: pages)
    return false;
  }
}

/**
 * Listens for and handles content events that we need for the
 * session store service to be notified of state changes in content.
 */
let EventListener = {

  DOM_EVENTS: [
    "pageshow", "change", "input"
  ],

  init: function () {
    this.DOM_EVENTS.forEach(e => addEventListener(e, this, true));
  },

  handleEvent: function (event) {
    switch (event.type) {
      case "pageshow":
        if (event.persisted && event.target == content.document)
          sendAsyncMessage("SessionStore:pageshow");
        break;
      case "input":
      case "change":
        sendAsyncMessage("SessionStore:input");
        break;
      default:
        debug("received unknown event '" + event.type + "'");
        break;
    }
  }
};

/**
 * Listens for and handles messages sent by the session store service.
 */
let MessageListener = {

  MESSAGES: [
    "SessionStore:collectSessionHistory"
  ],

  init: function () {
    this.MESSAGES.forEach(m => addMessageListener(m, this));
  },

  receiveMessage: function ({name, data: {id}}) {
    switch (name) {
      case "SessionStore:collectSessionHistory":
        let history = SessionHistory.collect(docShell);
        if ("index" in history) {
          let tabIndex = history.index - 1;
          // Don't include private data. It's only needed when duplicating
          // tabs, which collects data synchronously.
          TextAndScrollData.updateFrame(history.entries[tabIndex],
                                        content,
                                        docShell.isAppTab);
        }
        sendAsyncMessage(name, {id: id, data: history});
        break;
      default:
        debug("received unknown message '" + name + "'");
        break;
    }
  }
};

/**
 * If session data must be collected synchronously, we do it via
 * method calls to this object (rather than via messages to
 * MessageListener). When using multiple processes, these methods run
 * in the content process, but the parent synchronously waits on them
 * using cross-process object wrappers. Without multiple processes, we
 * still use this code for synchronous collection.
 */
let SyncHandler = {
  init: function () {
    // Send this object as a CPOW to chrome. In single-process mode,
    // the synchronous send ensures that the handler object is
    // available in SessionStore.jsm immediately upon loading
    // content-sessionStore.js.
    sendSyncMessage("SessionStore:setupSyncHandler", {}, {handler: this});
  },

  collectSessionHistory: function (includePrivateData) {
    let history = SessionHistory.collect(docShell);
    if ("index" in history) {
      let tabIndex = history.index - 1;
      TextAndScrollData.updateFrame(history.entries[tabIndex],
                                    content,
                                    docShell.isAppTab,
                                    {includePrivateData: includePrivateData});
    }
    return history;
  },

  /**
   * This function is used to make the tab process flush all data that
   * hasn't been sent to the parent process, yet.
   *
   * @param id (int)
   *        A unique id that represents the last message received by the chrome
   *        process before flushing. We will use this to determine data that
   *        would be lost when data has been sent asynchronously shortly
   *        before flushing synchronously.
   */
  flush: function (id) {
    MessageQueue.flush(id);
  },

  /**
   * DO NOT USE - DEBUGGING / TESTING ONLY
   *
   * This function is used to simulate certain situations where race conditions
   * can occur by sending data shortly before flushing synchronously.
   */
  flushAsync: function () {
    MessageQueue.flushAsync();
  }
};

let ProgressListener = {
  init: function() {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_LOCATION);
  },
  onLocationChange: function(aWebProgress, aRequest, aLocation, aFlags) {
    // We are changing page, so time to invalidate the state of the tab
    sendAsyncMessage("SessionStore:loadStart");
  },
  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {},
  onProgressChange: function() {},
  onStatusChange: function() {},
  onSecurityChange: function() {},
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference])
};

/**
 * Listens for scroll position changes. Whenever the user scrolls the top-most
 * frame we update the scroll position and will restore it when requested.
 *
 * Causes a SessionStore:update message to be sent that contains the current
 * scroll positions as a tree of strings. If no frame of the whole frame tree
 * is scrolled this will return null so that we don't tack a property onto
 * the tabData object in the parent process.
 *
 * Example:
 *   {scroll: "100,100", children: [null, null, {scroll: "200,200"}]}
 */
let ScrollPositionListener = {
  init: function () {
    addEventListener("scroll", this);
    gFrameTree.addObserver(this);
  },

  handleEvent: function (event) {
    let frame = event.target && event.target.defaultView;

    // Don't collect scroll data for frames created at or after the load event
    // as SessionStore can't restore scroll data for those.
    if (frame && gFrameTree.contains(frame)) {
      MessageQueue.push("scroll", () => this.collect());
    }
  },

  onFrameTreeCollected: function () {
    MessageQueue.push("scroll", () => this.collect());
  },

  onFrameTreeReset: function () {
    MessageQueue.push("scroll", () => null);
  },

  collect: function () {
    return gFrameTree.map(ScrollPosition.collect);
  }
};

/**
 * Listens for changes to the page style. Whenever a different page style is
 * selected or author styles are enabled/disabled we send a message with the
 * currently applied style to the chrome process.
 *
 * Causes a SessionStore:update message to be sent that contains the currently
 * selected pageStyle for all reachable frames.
 *
 * Example:
 *   {pageStyle: "Dusk", children: [null, {pageStyle: "Mozilla"}]}
 */
let PageStyleListener = {
  init: function () {
    Services.obs.addObserver(this, "author-style-disabled-changed", true);
    Services.obs.addObserver(this, "style-sheet-applicable-state-changed", true);
    gFrameTree.addObserver(this);
  },

  observe: function (subject, topic) {
    let frame = subject.defaultView;

    if (frame && gFrameTree.contains(frame)) {
      MessageQueue.push("pageStyle", () => this.collect());
    }
  },

  collect: function () {
    return PageStyle.collect(docShell, gFrameTree);
  },

  onFrameTreeCollected: function () {
    MessageQueue.push("pageStyle", () => this.collect());
  },

  onFrameTreeReset: function () {
    MessageQueue.push("pageStyle", () => null);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};

/**
 * Listens for changes to docShell capabilities. Whenever a new load is started
 * we need to re-check the list of capabilities and send message when it has
 * changed.
 *
 * Causes a SessionStore:update message to be sent that contains the currently
 * disabled docShell capabilities (all nsIDocShell.allow* properties set to
 * false) as a string - i.e. capability names separate by commas.
 */
let DocShellCapabilitiesListener = {
  /**
   * This field is used to compare the last docShell capabilities to the ones
   * that have just been collected. If nothing changed we won't send a message.
   */
  _latestCapabilities: "",

  init: function () {
    gFrameTree.addObserver(this);
  },

  /**
   * onFrameTreeReset() is called as soon as we start loading a page.
   */
  onFrameTreeReset: function() {
    // The order of docShell capabilities cannot change while we're running
    // so calling join() without sorting before is totally sufficient.
    let caps = DocShellCapabilities.collect(docShell).join(",");

    // Send new data only when the capability list changes.
    if (caps != this._latestCapabilities) {
      this._latestCapabilities = caps;
      MessageQueue.push("disallow", () => caps || null);
    }
  }
};

/**
 * Listens for changes to the DOMSessionStorage. Whenever new keys are added,
 * existing ones removed or changed, or the storage is cleared we will send a
 * message to the parent process containing up-to-date sessionStorage data.
 *
 * Causes a SessionStore:update message to be sent that contains the current
 * DOMSessionStorage contents. The data is a nested object using host names
 * as keys and per-host DOMSessionStorage data as values.
 */
let SessionStorageListener = {
  init: function () {
    addEventListener("MozStorageChanged", this);
    Services.obs.addObserver(this, "browser:purge-domain-data", true);
    gFrameTree.addObserver(this);
  },

  handleEvent: function (event) {
    // Ignore events triggered by localStorage or globalStorage changes.
    if (gFrameTree.contains(event.target) && isSessionStorageEvent(event)) {
      this.collect();
    }
  },

  observe: function () {
    // Collect data on the next tick so that any other observer
    // that needs to purge data can do its work first.
    setTimeout(() => this.collect(), 0);
  },

  collect: function () {
    if (docShell) {
      MessageQueue.push("storage", () => SessionStorage.collect(docShell, gFrameTree));
    }
  },

  onFrameTreeCollected: function () {
    this.collect();
  },

  onFrameTreeReset: function () {
    this.collect();
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};

/**
 * Listen for changes to the privacy status of the tab.
 * By definition, tabs start in non-private mode.
 *
 * Causes a SessionStore:update message to be sent for
 * field "isPrivate". This message contains
 *  |true| if the tab is now private
 *  |null| if the tab is now public - the field is therefore
 *  not saved.
 */
let PrivacyListener = {
  init: function() {
    docShell.addWeakPrivacyTransitionObserver(this);
  },

  // Ci.nsIPrivacyTransitionObserver
  privateModeChanged: function(enabled) {
    MessageQueue.push("isPrivate", () => enabled || null);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPrivacyTransitionObserver,
                                         Ci.nsISupportsWeakReference])
};

/**
 * A message queue that takes collected data and will take care of sending it
 * to the chrome process. It allows flushing using synchronous messages and
 * takes care of any race conditions that might occur because of that. Changes
 * will be batched if they're pushed in quick succession to avoid a message
 * flood.
 */
let MessageQueue = {
  /**
   * A unique, monotonically increasing ID used for outgoing messages. This is
   * important to make it possible to reuse tabs and allow sync flushes before
   * data could be destroyed.
   */
  _id: 1,

  /**
   * A map (string -> lazy fn) holding lazy closures of all queued data
   * collection routines. These functions will return data collected from the
   * docShell.
   */
  _data: new Map(),

  /**
   * A map holding the |this._id| value for every type of data back when it
   * was pushed onto the queue. We will use those IDs to find the data to send
   * and flush.
   */
  _lastUpdated: new Map(),

  /**
   * The delay (in ms) used to delay sending changes after data has been
   * invalidated.
   */
  BATCH_DELAY_MS: 1000,

  /**
   * The current timeout ID, null if there is no queue data. We use timeouts
   * to damp a flood of data changes and send lots of changes as one batch.
   */
  _timeout: null,

  /**
   * Pushes a given |value| onto the queue. The given |key| represents the type
   * of data that is stored and can override data that has been queued before
   * but has not been sent to the parent process, yet.
   *
   * @param key (string)
   *        A unique identifier specific to the type of data this is passed.
   * @param fn (function)
   *        A function that returns the value that will be sent to the parent
   *        process.
   */
  push: function (key, fn) {
    this._data.set(key, createLazy(fn));
    this._lastUpdated.set(key, this._id);

    if (!this._timeout) {
      // Wait a little before sending the message to batch multiple changes.
      this._timeout = setTimeout(() => this.send(), this.BATCH_DELAY_MS);
    }
  },

  /**
   * Sends queued data to the chrome process.
   *
   * @param options (object)
   *        {id: 123} to override the update ID used to accumulate data to send.
   *        {sync: true} to send data to the parent process synchronously.
   */
  send: function (options = {}) {
    // Looks like we have been called off a timeout after the tab has been
    // closed. The docShell is gone now and we can just return here as there
    // is nothing to do.
    if (!docShell) {
      return;
    }

    if (this._timeout) {
      clearTimeout(this._timeout);
      this._timeout = null;
    }

    let sync = options && options.sync;
    let startID = (options && options.id) || this._id;

    // We use sendRpcMessage in the sync case because we may have been called
    // through a CPOW. RPC messages are the only synchronous messages that the
    // child is allowed to send to the parent while it is handling a CPOW
    // request.
    let sendMessage = sync ? sendRpcMessage : sendAsyncMessage;

    let durationMs = Date.now();

    let data = {};
    for (let [key, id] of this._lastUpdated) {
      // There is no data for the given key anymore because
      // the parent process already marked it as received.
      if (!this._data.has(key)) {
        continue;
      }

      if (startID > id) {
        // If the |id| passed by the parent process is higher than the one
        // stored in |_lastUpdated| for the given key we know that the parent
        // received all necessary data and we can remove it from the map.
        this._data.delete(key);
        continue;
      }

      data[key] = this._data.get(key)();
    }

    durationMs = Date.now() - durationMs;
    let telemetry = {
      FX_SESSION_RESTORE_CONTENT_COLLECT_DATA_LONGEST_OP_MS: durationMs
    }

    // Send all data to the parent process.
    sendMessage("SessionStore:update", {
      id: this._id,
      data: data,
      telemetry: telemetry
    });

    // Increase our unique message ID.
    this._id++;
  },

  /**
   * This function is used to make the message queue flush all queue data that
   * hasn't been sent to the parent process, yet.
   *
   * @param id (int)
   *        A unique id that represents the latest message received by the
   *        chrome process. We can use this to determine which messages have not
   *        yet been received because they are still stuck in the event queue.
   */
  flush: function (id) {
    // It's important to always send data, even if there is nothing to flush.
    // The update message will be received by the parent process that can then
    // update its last received update ID to ignore stale messages.
    this.send({id: id + 1, sync: true});

    this._data.clear();
    this._lastUpdated.clear();
  },

  /**
   * DO NOT USE - DEBUGGING / TESTING ONLY
   *
   * This function is used to simulate certain situations where race conditions
   * can occur by sending data shortly before flushing synchronously.
   */
  flushAsync: function () {
    if (!Services.prefs.getBoolPref("browser.sessionstore.debug")) {
      throw new Error("flushAsync() must be used for testing, only.");
    }

    this.send();
  }
};

EventListener.init();
MessageListener.init();
SyncHandler.init();
ProgressListener.init();
PageStyleListener.init();
SessionStorageListener.init();
ScrollPositionListener.init();
DocShellCapabilitiesListener.init();
PrivacyListener.init();
