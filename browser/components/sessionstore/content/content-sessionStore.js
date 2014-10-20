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
XPCOMUtils.defineLazyModuleGetter(this, "FormData",
  "resource://gre/modules/FormData.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PageStyle",
  "resource:///modules/sessionstore/PageStyle.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ScrollPosition",
  "resource://gre/modules/ScrollPosition.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionHistory",
  "resource:///modules/sessionstore/SessionHistory.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionStorage",
  "resource:///modules/sessionstore/SessionStorage.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

Cu.import("resource:///modules/sessionstore/FrameTree.jsm", this);
let gFrameTree = new FrameTree(this);

Cu.import("resource:///modules/sessionstore/ContentRestore.jsm", this);
XPCOMUtils.defineLazyGetter(this, 'gContentRestore',
                            () => { return new ContentRestore(this) });

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
    return event.storageArea == event.target.sessionStorage;
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

  init: function () {
    addEventListener("load", this, true);
  },

  handleEvent: function (event) {
    // Ignore load events from subframes.
    if (event.target != content.document) {
      return;
    }

    // If we're in the process of restoring, this load may signal
    // the end of the restoration.
    let epoch = gContentRestore.getRestoreEpoch();
    if (!epoch) {
      return;
    }

    // Restore the form data and scroll position.
    gContentRestore.restoreDocument();

    // Ask SessionStore.jsm to trigger SSTabRestored.
    sendAsyncMessage("SessionStore:restoreDocumentComplete", {epoch: epoch});
  }
};

/**
 * Listens for and handles messages sent by the session store service.
 */
let MessageListener = {

  MESSAGES: [
    "SessionStore:restoreHistory",
    "SessionStore:restoreTabContent",
    "SessionStore:resetRestore",
  ],

  init: function () {
    this.MESSAGES.forEach(m => addMessageListener(m, this));
  },

  receiveMessage: function ({name, data}) {
    switch (name) {
      case "SessionStore:restoreHistory":
        let reloadCallback = () => {
          // Inform SessionStore.jsm about the reload. It will send
          // restoreTabContent in response.
          sendAsyncMessage("SessionStore:reloadPendingTab", {epoch: data.epoch});
        };
        gContentRestore.restoreHistory(data.epoch, data.tabData, reloadCallback);

        // When restoreHistory finishes, we send a synchronous message to
        // SessionStore.jsm so that it can run SSTabRestoring. Users of
        // SSTabRestoring seem to get confused if chrome and content are out of
        // sync about the state of the restore (particularly regarding
        // docShell.currentURI). Using a synchronous message is the easiest way
        // to temporarily synchronize them.
        sendSyncMessage("SessionStore:restoreHistoryComplete", {epoch: data.epoch});
        break;
      case "SessionStore:restoreTabContent":
        let epoch = gContentRestore.getRestoreEpoch();
        let finishCallback = () => {
          // Tell SessionStore.jsm that it may want to restore some more tabs,
          // since it restores a max of MAX_CONCURRENT_TAB_RESTORES at a time.
          sendAsyncMessage("SessionStore:restoreTabContentComplete", {epoch: epoch});
        };

        // We need to pass the value of didStartLoad back to SessionStore.jsm.
        let didStartLoad = gContentRestore.restoreTabContent(finishCallback);

        sendAsyncMessage("SessionStore:restoreTabContentStarted", {epoch: epoch});

        if (!didStartLoad) {
          // Pretend that the load succeeded so that event handlers fire correctly.
          sendAsyncMessage("SessionStore:restoreTabContentComplete", {epoch: epoch});
          sendAsyncMessage("SessionStore:restoreDocumentComplete", {epoch: epoch});
        }
        break;
      case "SessionStore:resetRestore":
        gContentRestore.resetRestore();
        break;
      default:
        debug("received unknown message '" + name + "'");
        break;
    }
  }
};

/**
 * On initialization, this handler gets sent to the parent process as a CPOW.
 * The parent will use it only to flush pending data from the frame script
 * when needed, i.e. when closing a tab, closing a window, shutting down, etc.
 *
 * This will hopefully not be needed in the future once we have async APIs for
 * closing windows and tabs.
 */
let SyncHandler = {
  init: function () {
    // Send this object as a CPOW to chrome. In single-process mode,
    // the synchronous send ensures that the handler object is
    // available in SessionStore.jsm immediately upon loading
    // content-sessionStore.js.
    sendSyncMessage("SessionStore:setupSyncHandler", {}, {handler: this});
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

/**
 * Listens for changes to the session history. Whenever the user navigates
 * we will collect URLs and everything belonging to session history.
 *
 * Causes a SessionStore:update message to be sent that contains the current
 * session history.
 *
 * Example:
 *   {entries: [{url: "about:mozilla", ...}, ...], index: 1}
 */
let SessionHistoryListener = {
  init: function () {
    // The frame tree observer is needed to handle initial subframe loads.
    // It will redundantly invalidate with the SHistoryListener in some cases
    // but these invalidations are very cheap.
    gFrameTree.addObserver(this);

    // By adding the SHistoryListener immediately, we will unfortunately be
    // notified of every history entry as the tab is restored. We don't bother
    // waiting to add the listener later because these notifications are cheap.
    // We will likely only collect once since we are batching collection on
    // a delay.
    docShell.QueryInterface(Ci.nsIWebNavigation).sessionHistory.
      addSHistoryListener(this);

    // Collect data if we start with a non-empty shistory.
    if (!SessionHistory.isEmpty(docShell)) {
      this.collect();
    }
  },

  uninit: function () {
    let sessionHistory = docShell.QueryInterface(Ci.nsIWebNavigation).sessionHistory;
    if (sessionHistory) {
      sessionHistory.removeSHistoryListener(this);
    }
  },

  collect: function () {
    if (docShell) {
      MessageQueue.push("history", () => SessionHistory.collect(docShell));
    }
  },

  onFrameTreeCollected: function () {
    this.collect();
  },

  onFrameTreeReset: function () {
    this.collect();
  },

  OnHistoryNewEntry: function (newURI) {
    this.collect();
  },

  OnHistoryGoBack: function (backURI) {
    this.collect();
    return true;
  },

  OnHistoryGoForward: function (forwardURI) {
    this.collect();
    return true;
  },

  OnHistoryGotoIndex: function (index, gotoURI) {
    this.collect();
    return true;
  },

  OnHistoryPurge: function (numEntries) {
    this.collect();
    return true;
  },

  OnHistoryReload: function (reloadURI, reloadFlags) {
    this.collect();
    return true;
  },

  OnHistoryReplaceEntry: function (index) {
    this.collect();
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsISHistoryListener,
    Ci.nsISupportsWeakReference
  ])
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
 * Listens for changes to input elements. Whenever the value of an input
 * element changes we will re-collect data for the current frame tree and send
 * a message to the parent process.
 *
 * Causes a SessionStore:update message to be sent that contains the form data
 * for all reachable frames.
 *
 * Example:
 *   {
 *     formdata: {url: "http://mozilla.org/", id: {input_id: "input value"}},
 *     children: [
 *       null,
 *       {url: "http://sub.mozilla.org/", id: {input_id: "input value 2"}}
 *     ]
 *   }
 */
let FormDataListener = {
  init: function () {
    addEventListener("input", this, true);
    addEventListener("change", this, true);
    gFrameTree.addObserver(this);
  },

  handleEvent: function (event) {
    let frame = event.target &&
                event.target.ownerDocument &&
                event.target.ownerDocument.defaultView;

    // Don't collect form data for frames created at or after the load event
    // as SessionStore can't restore form data for those.
    if (frame && gFrameTree.contains(frame)) {
      MessageQueue.push("formdata", () => this.collect());
    }
  },

  onFrameTreeReset: function () {
    MessageQueue.push("formdata", () => null);
  },

  collect: function () {
    return gFrameTree.map(FormData.collect);
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
    Services.obs.addObserver(this, "author-style-disabled-changed", false);
    Services.obs.addObserver(this, "style-sheet-applicable-state-changed", false);
    gFrameTree.addObserver(this);
  },

  uninit: function () {
    Services.obs.removeObserver(this, "author-style-disabled-changed");
    Services.obs.removeObserver(this, "style-sheet-applicable-state-changed");
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
  }
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
    addEventListener("MozStorageChanged", this, true);
    Services.obs.addObserver(this, "browser:purge-domain-data", false);
    gFrameTree.addObserver(this);
  },

  uninit: function () {
    Services.obs.removeObserver(this, "browser:purge-domain-data");
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
  }
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

    // Check that value at startup as it might have
    // been set before the frame script was loaded.
    if (docShell.QueryInterface(Ci.nsILoadContext).usePrivateBrowsing) {
      MessageQueue.push("isPrivate", () => true);
    }
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
FormDataListener.init();
SyncHandler.init();
PageStyleListener.init();
SessionHistoryListener.init();
SessionStorageListener.init();
ScrollPositionListener.init();
DocShellCapabilitiesListener.init();
PrivacyListener.init();

function handleRevivedTab() {
  if (content.document.documentURI.startsWith("about:tabcrashed")) {
    if (Services.appinfo.processType != Services.appinfo.PROCESS_TYPE_DEFAULT) {
      // Sanity check - we'd better be loading this in a non-remote browser.
      throw new Error("We seem to be navigating away from about:tabcrashed in " +
                      "a non-remote browser. This should really never happen.");
    }

    // We can't send a message using the frame message manager because by
    // the time we reach the unload event handler, it's "too late", and messages
    // won't be sent or received. The child-process message manager works though,
    // despite the fact that we're really running in the parent process.
    let browser = docShell.chromeEventHandler;
    cpmm.sendSyncMessage("SessionStore:RemoteTabRevived", null, {browser: browser});
  }
}

// If we're browsing from the tab crashed UI to a blacklisted URI that keeps
// this browser non-remote, we'll handle that in a pagehide event.
addEventListener("pagehide", handleRevivedTab);

addEventListener("unload", () => {
  // If we're browsing from the tab crashed UI to a URI that causes the tab
  // to go remote again, we catch this in the unload event handler, because
  // swapping out the non-remote browser for a remote one in
  // tabbrowser.xml's updateBrowserRemoteness doesn't cause the pagehide
  // event to be fired.
  handleRevivedTab();

  // Remove all registered nsIObservers.
  PageStyleListener.uninit();
  SessionStorageListener.uninit();
  SessionHistoryListener.uninit();

  // Remove progress listeners.
  gContentRestore.resetRestore();

  // We don't need to take care of any gFrameTree observers as the gFrameTree
  // will die with the content script. The same goes for the privacy transition
  // observer that will die with the docShell when the tab is closed.
});
