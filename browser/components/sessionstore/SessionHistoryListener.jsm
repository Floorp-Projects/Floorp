/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SessionHistoryListener"];

ChromeUtils.defineModuleGetter(
  this,
  "SessionHistory",
  "resource://gre/modules/sessionstore/SessionHistory.jsm"
);

const kNoIndex = Number.MAX_SAFE_INTEGER;
const kLastIndex = Number.MAX_SAFE_INTEGER - 1;

/**
 * Listens for state change notifcations from webProgress and notifies each
 * registered observer for either the start of a page load, or its completion.
 */
class StateChangeNotifier {
  constructor(store) {
    // super(store);
    this.store = store;

    this._observers = new Set();
    // let ifreq = this.mm.docShell.QueryInterface(Ci.nsIInterfaceRequestor);
    let ifreq = this.store.mm.docShell.QueryInterface(Ci.nsIInterfaceRequestor);
    let webProgress = ifreq.getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(
      this,
      Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
    );
  }

  get mm() {
    return this.store.mm;
  }

  /**
   * Adds a given observer |obs| to the set of observers that will be notified
   * when when a new document starts or finishes loading.
   *
   * @param obs (object)
   */
  addObserver(obs) {
    this._observers.add(obs);
  }

  /**
   * Notifies all observers that implement the given |method|.
   *
   * @param method (string)
   */
  notifyObservers(method) {
    for (let obs of this._observers) {
      if (typeof obs[method] == "function") {
        obs[method]();
      }
    }
  }

  /**
   * @see nsIWebProgressListener.onStateChange
   */
  onStateChange(webProgress, request, stateFlags, status) {
    // Ignore state changes for subframes because we're only interested in the
    // top-document starting or stopping its load.
    if (!webProgress.isTopLevel || webProgress.DOMWindow != this.mm.content) {
      return;
    }

    // onStateChange will be fired when loading the initial about:blank URI for
    // a browser, which we don't actually care about. This is particularly for
    // the case of unrestored background tabs, where the content has not yet
    // been restored: we don't want to accidentally send any updates to the
    // parent when the about:blank placeholder page has loaded.
    if (!this.mm.docShell.hasLoadedNonBlankURI) {
      return;
    }

    if (stateFlags & Ci.nsIWebProgressListener.STATE_START) {
      this.notifyObservers("onPageLoadStarted");
    } else if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
      this.notifyObservers("onPageLoadCompleted");
    }
  }
}
StateChangeNotifier.prototype.QueryInterface = ChromeUtils.generateQI([
  Ci.nsIWebProgressListener,
  Ci.nsISupportsWeakReference,
]);

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
class SessionHistoryListener {
  constructor(store) {
    // super(store);
    this.store = store;

    this._fromIdx = kNoIndex;

    // The state change observer is needed to handle initial subframe loads.
    // It will redundantly invalidate with the SHistoryListener in some cases
    // but these invalidations are very cheap.
    this.stateChangeNotifier = new StateChangeNotifier(this);
    this.stateChangeNotifier.addObserver(this);

    // By adding the SHistoryListener immediately, we will unfortunately be
    // notified of every history entry as the tab is restored. We don't bother
    // waiting to add the listener later because these notifications are cheap.
    // We will likely only collect once since we are batching collection on
    // a delay.
    this.store.mm.docShell
      .QueryInterface(Ci.nsIWebNavigation)
      .sessionHistory.legacySHistory.addSHistoryListener(this);

    // Collect data if we start with a non-empty shistory.
    if (!SessionHistory.isEmpty(this.store.mm.docShell)) {
      this.collect();
      // When a tab is detached from the window, for the new window there is a
      // new SessionHistoryListener created. Normally it is empty at this point
      // but in a test env. the initial about:blank might have a children in which
      // case we fire off a history message here with about:blank in it. If we
      // don't do it ASAP then there is going to be a browser swap and the parent
      // will be all confused by that message.
      this.store.messageQueue.send();
    }

    // Listen for page title changes.
    this.store.mm.addEventListener("DOMTitleChanged", this);
  }

  get mm() {
    return this.store.mm;
  }

  uninit() {
    let sessionHistory = this.mm.docShell.QueryInterface(Ci.nsIWebNavigation)
      .sessionHistory;
    if (sessionHistory) {
      sessionHistory.legacySHistory.removeSHistoryListener(this);
    }
  }

  collect() {
    // We want to send down a historychange even for full collects in case our
    // session history is a partial session history, in which case we don't have
    // enough information for a full update. collectFrom(-1) tells the collect
    // function to collect all data avaliable in this process.
    if (this.mm.docShell) {
      this.collectFrom(-1);
    }
  }

  // History can grow relatively big with the nested elements, so if we don't have to, we
  // don't want to send the entire history all the time. For a simple optimization
  // we keep track of the smallest index from after any change has occured and we just send
  // the elements from that index. If something more complicated happens we just clear it
  // and send the entire history. We always send the additional info like the current selected
  // index (so for going back and forth between history entries we set the index to kLastIndex
  // if nothing else changed send an empty array and the additonal info like the selected index)
  collectFrom(idx) {
    if (this._fromIdx <= idx) {
      // If we already know that we need to update history fromn index N we can ignore any changes
      // tha happened with an element with index larger than N.
      // Note: initially we use kNoIndex which is MAX_SAFE_INTEGER which means we don't ignore anything
      // here, and in case of navigation in the history back and forth we use kLastIndex which ignores
      // only the subsequent navigations, but not any new elements added.
      return;
    }

    this._fromIdx = idx;
    this.store.messageQueue.push("historychange", () => {
      if (this._fromIdx === kNoIndex) {
        return null;
      }

      let history = SessionHistory.collect(this.mm.docShell, this._fromIdx);
      this._fromIdx = kNoIndex;
      return history;
    });
  }

  handleEvent(event) {
    this.collect();
  }

  onPageLoadCompleted() {
    this.collect();
  }

  onPageLoadStarted() {
    this.collect();
  }

  OnHistoryNewEntry(newURI, oldIndex) {
    // We ought to collect the previously current entry as well, see bug 1350567.
    this.collectFrom(oldIndex);
  }

  OnHistoryGotoIndex() {
    // We ought to collect the previously current entry as well, see bug 1350567.
    this.collectFrom(kLastIndex);
  }

  OnHistoryPurge() {
    this.collect();
  }

  OnHistoryReload() {
    this.collect();
    return true;
  }

  OnHistoryReplaceEntry() {
    this.collect();
  }
}
SessionHistoryListener.prototype.QueryInterface = ChromeUtils.generateQI([
  Ci.nsISHistoryListener,
  Ci.nsISupportsWeakReference,
]);
