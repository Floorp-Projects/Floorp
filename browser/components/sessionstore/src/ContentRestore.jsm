/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ContentRestore"];

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

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
XPCOMUtils.defineLazyModuleGetter(this, "Utils",
  "resource:///modules/sessionstore/Utils.jsm");

/**
 * This module implements the content side of session restoration. The chrome
 * side is handled by SessionStore.jsm. The functions in this module are called
 * by content-sessionStore.js based on messages received from SessionStore.jsm
 * (or, in one case, based on a "load" event). Each tab has its own
 * ContentRestore instance, constructed by content-sessionStore.js.
 *
 * In a typical restore, content-sessionStore.js will call the following based
 * on messages and events it receives:
 *
 *   restoreHistory(epoch, tabData, reloadCallback)
 *     Restores the tab's history and session cookies.
 *   restoreTabContent(finishCallback)
 *     Starts loading the data for the current page to restore.
 *   restoreDocument()
 *     Restore form and scroll data.
 *
 * When the page has been loaded from the network, we call finishCallback. It
 * should send a message to SessionStore.jsm, which may cause other tabs to be
 * restored.
 *
 * When the page has finished loading, a "load" event will trigger in
 * content-sessionStore.js, which will call restoreDocument. At that point,
 * form data is restored and the restore is complete.
 *
 * At any time, SessionStore.jsm can cancel the ongoing restore by sending a
 * reset message, which causes resetRestore to be called. At that point it's
 * legal to begin another restore.
 *
 * The epoch that is passed into restoreHistory is merely a token. All messages
 * sent back to SessionStore.jsm include the epoch. This way, SessionStore.jsm
 * can discard messages that relate to restores that it has canceled (by
 * starting a new restore, say).
 */
function ContentRestore(chromeGlobal) {
  let internal = new ContentRestoreInternal(chromeGlobal);
  let external = {};

  let EXPORTED_METHODS = ["restoreHistory",
                          "restoreTabContent",
                          "restoreDocument",
                          "resetRestore",
                          "getRestoreEpoch",
                         ];

  for (let method of EXPORTED_METHODS) {
    external[method] = internal[method].bind(internal);
  }

  return Object.freeze(external);
}

function ContentRestoreInternal(chromeGlobal) {
  this.chromeGlobal = chromeGlobal;

  // The following fields are only valid during certain phases of the restore
  // process.

  // The epoch that was passed into restoreHistory. Removed in restoreDocument.
  this._epoch = 0;

  // The tabData for the restore. Set in restoreHistory and removed in
  // restoreTabContent.
  this._tabData = null;

  // Contains {entry, pageStyle, scrollPositions}, where entry is a
  // single entry from the tabData.entries array. Set in
  // restoreTabContent and removed in restoreDocument.
  this._restoringDocument = null;

  // This listener is used to detect reloads on restoring tabs. Set in
  // restoreHistory and removed in restoreTabContent.
  this._historyListener = null;

  // This listener detects when a restoring tab has finished loading data from
  // the network. Set in restoreTabContent and removed in resetRestore.
  this._progressListener = null;
}

/**
 * The API for the ContentRestore module. Methods listed in EXPORTED_METHODS are
 * public.
 */
ContentRestoreInternal.prototype = {

  get docShell() {
    return this.chromeGlobal.docShell;
  },

  /**
   * Starts the process of restoring a tab. The tabData to be restored is passed
   * in here and used throughout the restoration. The epoch (which must be
   * non-zero) is passed through to all the callbacks. If the tab is ever
   * reloaded during the restore process, reloadCallback is called.
   */
  restoreHistory: function (epoch, tabData, reloadCallback) {
    this._tabData = tabData;
    this._epoch = epoch;

    // In case about:blank isn't done yet.
    let webNavigation = this.docShell.QueryInterface(Ci.nsIWebNavigation);
    webNavigation.stop(Ci.nsIWebNavigation.STOP_ALL);

    // Make sure currentURI is set so that switch-to-tab works before the tab is
    // restored. We'll reset this to about:blank when we try to restore the tab
    // to ensure that docshell doeesn't get confused.
    let activeIndex = tabData.index - 1;
    let activePageData = tabData.entries[activeIndex] || {};
    let uri = activePageData.url || null;
    if (uri) {
      webNavigation.setCurrentURI(Utils.makeURI(uri));
    }

    SessionHistory.restore(this.docShell, tabData);

    // Add a listener to watch for reloads.
    let webNavigation = this.docShell.QueryInterface(Ci.nsIWebNavigation);
    let listener = new HistoryListener(this.docShell, reloadCallback);
    webNavigation.sessionHistory.addSHistoryListener(listener);
    this._historyListener = listener;

    // Make sure to reset the capabilities and attributes in case this tab gets
    // reused.
    let disallow = new Set(tabData.disallow && tabData.disallow.split(","));
    DocShellCapabilities.restore(this.docShell, disallow);

    if (tabData.storage && this.docShell instanceof Ci.nsIDocShell)
      SessionStorage.restore(this.docShell, tabData.storage);
  },

  /**
   * Start loading the current page. When the data has finished loading from the
   * network, finishCallback is called. Returns true if the load was successful.
   */
  restoreTabContent: function (finishCallback) {
    let tabData = this._tabData;
    this._tabData = null;

    let webNavigation = this.docShell.QueryInterface(Ci.nsIWebNavigation);
    let history = webNavigation.sessionHistory;

    // The reload listener is no longer needed.
    this._historyListener.uninstall();
    this._historyListener = null;

    // We're about to start a load. This listener will be called when the load
    // has finished getting everything from the network.
    let progressListener = new ProgressListener(this.docShell, () => {
      // Call resetRestore to reset the state back to normal. The data needed
      // for restoreDocument (which hasn't happened yet) will remain in
      // _restoringDocument.
      this.resetRestore(this.docShell);

      finishCallback();
    });
    this._progressListener = progressListener;

    // Reset the current URI to about:blank. We changed it above for
    // switch-to-tab, but now it must go back to the correct value before the
    // load happens.
    webNavigation.setCurrentURI(Utils.makeURI("about:blank"));

    try {
      if (tabData.userTypedValue && tabData.userTypedClear) {
        // If the user typed a URL into the URL bar and hit enter right before
        // we crashed, we want to start loading that page again. A non-zero
        // userTypedClear value means that the load had started.
        let activeIndex = tabData.index - 1;
        if (activeIndex > 0) {
          // Go to the right history entry, but don't load anything yet.
          history.getEntryAtIndex(activeIndex, true);
        }

        // Load userTypedValue and fix up the URL if it's partial/broken.
        webNavigation.loadURI(tabData.userTypedValue,
                              Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP,
                              null, null, null);
      } else if (tabData.entries.length) {
        // Stash away the data we need for restoreDocument.
        let activeIndex = tabData.index - 1;
        this._restoringDocument = {entry: tabData.entries[activeIndex] || {},
                                   pageStyle: tabData.pageStyle || {},
                                   scrollPositions: tabData.scroll || {}};

        // In order to work around certain issues in session history, we need to
        // force session history to update its internal index and call reload
        // instead of gotoIndex. See bug 597315.
        history.getEntryAtIndex(activeIndex, true);
        history.reloadCurrentEntry();
      } else {
        // If there's nothing to restore, we should still blank the page.
        webNavigation.loadURI("about:blank",
                              Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY,
                              null, null, null);
      }

      return true;
    } catch (ex if ex instanceof Ci.nsIException) {
      // Ignore page load errors, but return false to signal that the load never
      // happened.
      return false;
    }
  },

  /**
   * Accumulates a list of frames that need to be restored for the given browser
   * element. A frame is only restored if its current URL matches the one saved
   * in the session data. Each frame to be restored is returned along with its
   * associated session data.
   *
   * @param browser the browser being restored
   * @return an array of [frame, data] pairs
   */
  getFramesToRestore: function (content, data) {
    function hasExpectedURL(aDocument, aURL) {
      return !aURL || aURL.replace(/#.*/, "") == aDocument.location.href.replace(/#.*/, "");
    }

    let frameList = [];

    function enumerateFrame(content, data) {
      // Skip the frame if the user has navigated away before loading finished.
      if (!hasExpectedURL(content.document, data.url)) {
        return;
      }

      frameList.push([content, data]);

      for (let i = 0; i < content.frames.length; i++) {
        if (data.children && data.children[i]) {
          enumerateFrame(content.frames[i], data.children[i]);
        }
      }
    }

    enumerateFrame(content, data);

    return frameList;
  },

  /**
   * Finish restoring the tab by filling in form data and setting the scroll
   * position. The restore is complete when this function exits. It should be
   * called when the "load" event fires for the restoring tab.
   */
  restoreDocument: function () {
    this._epoch = 0;

    if (!this._restoringDocument) {
      return;
    }
    let {entry, pageStyle, scrollPositions} = this._restoringDocument;
    this._restoringDocument = null;

    let window = this.docShell.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
    let frameList = this.getFramesToRestore(window, entry);

    // Support the old pageStyle format.
    if (typeof(pageStyle) === "string") {
      PageStyle.restore(this.docShell, frameList, pageStyle);
    } else {
      PageStyle.restoreTree(this.docShell, pageStyle);
    }

    ScrollPosition.restoreTree(window, scrollPositions);
    TextAndScrollData.restore(frameList);
  },

  /**
   * Cancel an ongoing restore. This function can be called any time between
   * restoreHistory and restoreDocument.
   *
   * This function is called externally (if a restore is canceled) and
   * internally (when the loads for a restore have finished). In the latter
   * case, it's called before restoreDocument, so it cannot clear
   * _restoringDocument.
   */
  resetRestore: function () {
    this._tabData = null;

    if (this._historyListener) {
      this._historyListener.uninstall();
    }
    this._historyListener = null;

    if (this._progressListener) {
      this._progressListener.uninstall();
    }
    this._progressListener = null;
  },

  /**
   * If a restore is ongoing, this function returns the value of |epoch| that
   * was passed to restoreHistory. If no restore is ongoing, it returns 0.
   */
  getRestoreEpoch: function () {
    return this._epoch;
  },
};

/*
 * This listener detects when a page being restored is reloaded. It triggers a
 * callback and cancels the reload. The callback will send a message to
 * SessionStore.jsm so that it can restore the content immediately.
 */
function HistoryListener(docShell, callback) {
  let webNavigation = docShell.QueryInterface(Ci.nsIWebNavigation);
  webNavigation.sessionHistory.addSHistoryListener(this);

  this.webNavigation = webNavigation;
  this.callback = callback;
}
HistoryListener.prototype = {
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsISHistoryListener,
    Ci.nsISupportsWeakReference
  ]),

  uninstall: function () {
    this.webNavigation.sessionHistory.removeSHistoryListener(this);
  },

  OnHistoryNewEntry: function(newURI) {},
  OnHistoryGoBack: function(backURI) { return true; },
  OnHistoryGoForward: function(forwardURI) { return true; },
  OnHistoryGotoIndex: function(index, gotoURI) { return true; },
  OnHistoryPurge: function(numEntries) { return true; },

  OnHistoryReload: function(reloadURI, reloadFlags) {
    this.callback();

    // Cancel the load.
    return false;
  },
}

/**
 * This class informs SessionStore.jsm whenever the network requests for a
 * restoring page have completely finished. We only restore three tabs
 * simultaneously, so this is the signal for SessionStore.jsm to kick off
 * another restore (if there are more to do).
 */
function ProgressListener(docShell, callback)
{
  let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebProgress);
  webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_STATE_WINDOW);

  this.webProgress = webProgress;
  this.callback = callback;
}
ProgressListener.prototype = {
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIWebProgressListener,
    Ci.nsISupportsWeakReference
  ]),

  uninstall: function() {
    this.webProgress.removeProgressListener(this);
  },

  onStateChange: function(webProgress, request, stateFlags, status) {
    if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
        stateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
        stateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) {
      this.callback();
    }
  },

  onLocationChange: function() {},
  onProgressChange: function() {},
  onStatusChange: function() {},
  onSecurityChange: function() {},
};
