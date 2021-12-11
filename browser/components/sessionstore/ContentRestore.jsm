/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ContentRestore"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "SessionHistory",
  "resource://gre/modules/sessionstore/SessionHistory.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Utils",
  "resource://gre/modules/sessionstore/Utils.jsm"
);
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
 *   restoreHistory(tabData, loadArguments, callbacks)
 *     Restores the tab's history and session cookies.
 *   restoreTabContent(loadArguments, finishCallback)
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
 */
function ContentRestore(chromeGlobal) {
  let internal = new ContentRestoreInternal(chromeGlobal);
  let external = {};

  let EXPORTED_METHODS = [
    "restoreHistory",
    "restoreTabContent",
    "restoreDocument",
    "resetRestore",
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

  // The tabData for the restore. Set in restoreHistory and removed in
  // restoreTabContent.
  this._tabData = null;

  // Contains {entry, scrollPositions, formdata}, where entry is a
  // single entry from the tabData.entries array. Set in
  // restoreTabContent and removed in restoreDocument.
  this._restoringDocument = null;

  // This listener is used to detect reloads on restoring tabs. Set in
  // restoreHistory and removed in restoreTabContent.
  this._historyListener = null;

  // This listener detects when a pending tab starts loading (when not
  // initiated by sessionstore) and when a restoring tab has finished loading
  // data from the network. Set in restoreHistory() and restoreTabContent(),
  // removed in resetRestore().
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
   * non-zero) is passed through to all the callbacks. If a load in the tab
   * is started while it is pending, the appropriate callbacks are called.
   */
  restoreHistory(tabData, loadArguments, callbacks) {
    this._tabData = tabData;

    // In case about:blank isn't done yet.
    let webNavigation = this.docShell.QueryInterface(Ci.nsIWebNavigation);
    webNavigation.stop(Ci.nsIWebNavigation.STOP_ALL);

    // Make sure currentURI is set so that switch-to-tab works before the tab is
    // restored. We'll reset this to about:blank when we try to restore the tab
    // to ensure that docshell doeesn't get confused. Don't bother doing this if
    // we're restoring immediately due to a process switch. It just causes the
    // URL bar to be temporarily blank.
    let activeIndex = tabData.index - 1;
    let activePageData = tabData.entries[activeIndex] || {};
    let uri = activePageData.url || null;
    if (uri && !loadArguments) {
      webNavigation.setCurrentURI(Services.io.newURI(uri));
    }

    SessionHistory.restore(this.docShell, tabData);

    // Add a listener to watch for reloads.
    let listener = new HistoryListener(this.docShell, () => {
      // On reload, restore tab contents.
      this.restoreTabContent(null, false, callbacks.onLoadFinished);
    });

    webNavigation.sessionHistory.legacySHistory.addSHistoryListener(listener);
    this._historyListener = listener;

    // Make sure to reset the capabilities and attributes in case this tab gets
    // reused.
    SessionStoreUtils.restoreDocShellCapabilities(
      this.docShell,
      tabData.disallow
    );

    // Add a progress listener to correctly handle browser.loadURI()
    // calls from foreign code.
    this._progressListener = new ProgressListener(this.docShell, {
      onStartRequest: () => {
        // Some code called browser.loadURI() on a pending tab. It's safe to
        // assume we don't care about restoring scroll or form data.
        this._tabData = null;

        // Listen for the tab to finish loading.
        this.restoreTabContentStarted(callbacks.onLoadFinished);

        // Notify the parent.
        callbacks.onLoadStarted();
      },
    });
  },

  /**
   * Start loading the current page. When the data has finished loading from the
   * network, finishCallback is called. Returns true if the load was successful.
   */
  restoreTabContent(loadArguments, isRemotenessUpdate, finishCallback) {
    let tabData = this._tabData;
    this._tabData = null;

    let webNavigation = this.docShell.QueryInterface(Ci.nsIWebNavigation);

    // Listen for the tab to finish loading.
    this.restoreTabContentStarted(finishCallback);

    // Reset the current URI to about:blank. We changed it above for
    // switch-to-tab, but now it must go back to the correct value before the
    // load happens. Don't bother doing this if we're restoring immediately
    // due to a process switch.
    if (!isRemotenessUpdate) {
      webNavigation.setCurrentURI(Services.io.newURI("about:blank"));
    }

    try {
      if (loadArguments) {
        // If the load was started in another process, and the in-flight channel
        // was redirected into this process, resume that load within our process.
        //
        // NOTE: In this case `isRemotenessUpdate` must be true.
        webNavigation.resumeRedirectedLoad(
          loadArguments.redirectLoadSwitchId,
          loadArguments.redirectHistoryIndex
        );
      } else if (tabData.userTypedValue && tabData.userTypedClear) {
        // If the user typed a URL into the URL bar and hit enter right before
        // we crashed, we want to start loading that page again. A non-zero
        // userTypedClear value means that the load had started.
        // Load userTypedValue and fix up the URL if it's partial/broken.
        let loadURIOptions = {
          triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
          loadFlags: Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP,
        };
        webNavigation.loadURI(tabData.userTypedValue, loadURIOptions);
      } else if (tabData.entries.length) {
        // Stash away the data we need for restoreDocument.
        this._restoringDocument = {
          formdata: tabData.formdata || {},
          scrollPositions: tabData.scroll || {},
        };

        // In order to work around certain issues in session history, we need to
        // force session history to update its internal index and call reload
        // instead of gotoIndex. See bug 597315.
        let history = webNavigation.sessionHistory.legacySHistory;
        history.reloadCurrentEntry();
      } else {
        // If there's nothing to restore, we should still blank the page.
        let loadURIOptions = {
          triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
          loadFlags: Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY,
          // Specify an override to force the load to finish in the current
          // process, as tests rely on this behaviour for non-fission session
          // restore.
          remoteTypeOverride: Services.appinfo.remoteType,
        };
        webNavigation.loadURI("about:blank", loadURIOptions);
      }

      return true;
    } catch (ex) {
      if (ex instanceof Ci.nsIException) {
        // Ignore page load errors, but return false to signal that the load never
        // happened.
        return false;
      }
    }
    return null;
  },

  /**
   * To be called after restoreHistory(). Removes all listeners needed for
   * pending tabs and makes sure to notify when the tab finished loading.
   */
  restoreTabContentStarted(finishCallback) {
    // The reload listener is no longer needed.
    this._historyListener.uninstall();
    this._historyListener = null;

    // Remove the old progress listener.
    this._progressListener.uninstall();

    // We're about to start a load. This listener will be called when the load
    // has finished getting everything from the network.
    this._progressListener = new ProgressListener(this.docShell, {
      onStopRequest: () => {
        // Call resetRestore() to reset the state back to normal. The data
        // needed for restoreDocument() (which hasn't happened yet) will
        // remain in _restoringDocument.
        this.resetRestore();

        finishCallback();
      },
    });
  },

  /**
   * Finish restoring the tab by filling in form data and setting the scroll
   * position. The restore is complete when this function exits. It should be
   * called when the "load" event fires for the restoring tab.  Returns true
   * if we're restoring a document.
   */
  restoreDocument() {
    if (!this._restoringDocument) {
      return;
    }

    let { formdata, scrollPositions } = this._restoringDocument;
    this._restoringDocument = null;

    let window = this.docShell.domWindow;

    // Restore form data.
    Utils.restoreFrameTreeData(window, formdata, (frame, data) => {
      // restore() will return false, and thus abort restoration for the
      // current |frame| and its descendants, if |data.url| is given but
      // doesn't match the loaded document's URL.
      return SessionStoreUtils.restoreFormData(frame.document, data);
    });

    // Restore scroll data.
    Utils.restoreFrameTreeData(window, scrollPositions, (frame, data) => {
      if (data.scroll) {
        SessionStoreUtils.restoreScrollPosition(frame, data);
      }
    });
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
  resetRestore() {
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
};

/*
 * This listener detects when a page being restored is reloaded. It triggers a
 * callback and cancels the reload. The callback will send a message to
 * SessionStore.jsm so that it can restore the content immediately.
 */
function HistoryListener(docShell, callback) {
  let webNavigation = docShell.QueryInterface(Ci.nsIWebNavigation);
  webNavigation.sessionHistory.legacySHistory.addSHistoryListener(this);

  this.webNavigation = webNavigation;
  this.callback = callback;
}
HistoryListener.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    "nsISHistoryListener",
    "nsISupportsWeakReference",
  ]),

  uninstall() {
    let shistory = this.webNavigation.sessionHistory.legacySHistory;
    if (shistory) {
      shistory.removeSHistoryListener(this);
    }
  },

  OnHistoryGotoIndex() {},
  OnHistoryPurge() {},
  OnHistoryReplaceEntry() {},

  // This will be called for a pending tab when loadURI(uri) is called where
  // the given |uri| only differs in the fragment.
  OnHistoryNewEntry(newURI) {
    let currentURI = this.webNavigation.currentURI;

    // Ignore new SHistory entries with the same URI as those do not indicate
    // a navigation inside a document by changing the #hash part of the URL.
    // We usually hit this when purging session history for browsers.
    if (currentURI && currentURI.spec == newURI.spec) {
      return;
    }

    // Reset the tab's URL to what it's actually showing. Without this loadURI()
    // would use the current document and change the displayed URL only.
    this.webNavigation.setCurrentURI(Services.io.newURI("about:blank"));

    // Kick off a new load so that we navigate away from about:blank to the
    // new URL that was passed to loadURI(). The new load will cause a
    // STATE_START notification to be sent and the ProgressListener will then
    // notify the parent and do the rest.
    let loadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
    let loadURIOptions = {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      loadFlags,
    };
    this.webNavigation.loadURI(newURI.spec, loadURIOptions);
  },

  OnHistoryReload() {
    this.callback();

    // Cancel the load.
    return false;
  },
};

/**
 * This class informs SessionStore.jsm whenever the network requests for a
 * restoring page have completely finished. We only restore three tabs
 * simultaneously, so this is the signal for SessionStore.jsm to kick off
 * another restore (if there are more to do).
 *
 * The progress listener is also used to be notified when a load not initiated
 * by sessionstore starts. Pending tabs will then need to be marked as no
 * longer pending.
 */
function ProgressListener(docShell, callbacks) {
  let webProgress = docShell
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebProgress);
  webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_STATE_WINDOW);

  this.webProgress = webProgress;
  this.callbacks = callbacks;
}

ProgressListener.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIWebProgressListener",
    "nsISupportsWeakReference",
  ]),

  uninstall() {
    this.webProgress.removeProgressListener(this);
  },

  onStateChange(webProgress, request, stateFlags, status) {
    let {
      STATE_IS_WINDOW,
      STATE_STOP,
      STATE_START,
    } = Ci.nsIWebProgressListener;
    if (!webProgress.isTopLevel || !(stateFlags & STATE_IS_WINDOW)) {
      return;
    }

    if (stateFlags & STATE_START && this.callbacks.onStartRequest) {
      this.callbacks.onStartRequest();
    }

    if (stateFlags & STATE_STOP && this.callbacks.onStopRequest) {
      this.callbacks.onStopRequest();
    }
  },
};
