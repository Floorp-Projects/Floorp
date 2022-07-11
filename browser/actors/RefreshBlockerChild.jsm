/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file has two actors, RefreshBlockerChild js a window actor which
 * handles the refresh notifications. RefreshBlockerObserverChild is a process
 * actor that enables refresh blocking on each docshell that is created.
 */

var EXPORTED_SYMBOLS = ["RefreshBlockerChild", "RefreshBlockerObserverChild"];

const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

const REFRESHBLOCKING_PREF = "accessibility.blockautorefresh";

var progressListener = {
  // Bug 1247100 - When a refresh is caused by an HTTP header,
  // onRefreshAttempted will be fired before onLocationChange.
  // When a refresh is caused by a <meta> tag in the document,
  // onRefreshAttempted will be fired after onLocationChange.
  //
  // We only ever want to send a message to the parent after
  // onLocationChange has fired, since the parent uses the
  // onLocationChange update to clear transient notifications.
  // Sending the message before onLocationChange will result in
  // us creating the notification, and then clearing it very
  // soon after.
  //
  // To account for both cases (onRefreshAttempted before
  // onLocationChange, and onRefreshAttempted after onLocationChange),
  // we'll hold a mapping of DOM Windows that we see get
  // sent through both onLocationChange and onRefreshAttempted.
  // When either run, they'll check the WeakMap for the existence
  // of the DOM Window. If it doesn't exist, it'll add it. If
  // it finds it, it'll know that it's safe to send the message
  // to the parent, since we know that both have fired.
  //
  // The DOM Window is removed from blockedWindows when we notice
  // the nsIWebProgress change state to STATE_STOP for the
  // STATE_IS_WINDOW case.
  //
  // DOM Windows are mapped to a JS object that contains the data
  // to be sent to the parent to show the notification. Since that
  // data is only known when onRefreshAttempted is fired, it's only
  // ever stashed in the map if onRefreshAttempted fires first -
  // otherwise, null is set as the value of the mapping.
  blockedWindows: new WeakMap(),

  /**
   * Notices when the nsIWebProgress transitions to STATE_STOP for
   * the STATE_IS_WINDOW case, which will clear any mappings from
   * blockedWindows.
   */
  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (
      aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW &&
      aStateFlags & Ci.nsIWebProgressListener.STATE_STOP
    ) {
      this.blockedWindows.delete(aWebProgress.DOMWindow);
    }
  },

  /**
   * Notices when the location has changed. If, when running,
   * onRefreshAttempted has already fired for this DOM Window, will
   * send the appropriate refresh blocked data to the parent.
   */
  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    let win = aWebProgress.DOMWindow;
    if (this.blockedWindows.has(win)) {
      let data = this.blockedWindows.get(win);
      if (data) {
        // We saw onRefreshAttempted before onLocationChange, so
        // send the message to the parent to show the notification.
        this.send(win, data);
      }
    } else {
      this.blockedWindows.set(win, null);
    }
  },

  /**
   * Notices when a refresh / reload was attempted. If, when running,
   * onLocationChange has not yet run, will stash the appropriate data
   * into the blockedWindows map to be sent when onLocationChange fires.
   */
  onRefreshAttempted(aWebProgress, aURI, aDelay, aSameURI) {
    let win = aWebProgress.DOMWindow;

    let data = {
      browsingContext: win.browsingContext,
      URI: aURI.spec,
      delay: aDelay,
      sameURI: aSameURI,
    };

    if (this.blockedWindows.has(win)) {
      // onLocationChange must have fired before, so we can tell the
      // parent to show the notification.
      this.send(win, data);
    } else {
      // onLocationChange hasn't fired yet, so stash the data in the
      // map so that onLocationChange can send it when it fires.
      this.blockedWindows.set(win, data);
    }

    return false;
  },

  send(win, data) {
    // Due to the |nsDocLoader| calling its |nsIWebProgressListener|s in
    // reverse order, this will occur *before* the |BrowserChild| can send its
    // |OnLocationChange| event to the parent, but we need this message to
    // arrive after to ensure that the refresh blocker notification is not
    // immediately cleared by the |OnLocationChange| from |BrowserChild|.
    setTimeout(() => {
      // An exception can occur if refresh blocking was turned off
      // during a pageload.
      try {
        let actor = win.windowGlobalChild.getActor("RefreshBlocker");
        if (actor) {
          actor.sendAsyncMessage("RefreshBlocker:Blocked", data);
        }
      } catch (ex) {}
    }, 0);
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIWebProgressListener2",
    "nsIWebProgressListener",
    "nsISupportsWeakReference",
  ]),
};

class RefreshBlockerChild extends JSWindowActorChild {
  didDestroy() {
    // If the refresh blocking preference is turned off, all of the
    // RefreshBlockerChild actors will get destroyed, so disable
    // refresh blocking only in this case.
    if (!Services.prefs.getBoolPref(REFRESHBLOCKING_PREF)) {
      this.disable(this.docShell);
    }
  }

  enable() {
    ChromeUtils.domProcessChild
      .getActor("RefreshBlockerObserver")
      .enable(this.docShell);
  }

  disable() {
    ChromeUtils.domProcessChild
      .getActor("RefreshBlockerObserver")
      .disable(this.docShell);
  }

  receiveMessage(message) {
    let data = message.data;

    switch (message.name) {
      case "RefreshBlocker:Refresh":
        let docShell = data.browsingContext.docShell;
        let refreshURI = docShell.QueryInterface(Ci.nsIRefreshURI);
        let URI = Services.io.newURI(data.URI);
        refreshURI.forceRefreshURI(URI, null, data.delay);
        break;

      case "PreferenceChanged":
        if (data.isEnabled) {
          this.enable(this.docShell);
        } else {
          this.disable(this.docShell);
        }
    }
  }
}

class RefreshBlockerObserverChild extends JSProcessActorChild {
  constructor() {
    super();
    this.filtersMap = new Map();
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "webnavigation-create":
      case "chrome-webnavigation-create":
        if (Services.prefs.getBoolPref(REFRESHBLOCKING_PREF)) {
          this.enable(subject.QueryInterface(Ci.nsIDocShell));
        }
        break;

      case "webnavigation-destroy":
      case "chrome-webnavigation-destroy":
        if (Services.prefs.getBoolPref(REFRESHBLOCKING_PREF)) {
          this.disable(subject.QueryInterface(Ci.nsIDocShell));
        }
        break;
    }
  }

  enable(docShell) {
    if (this.filtersMap.has(docShell)) {
      return;
    }

    let filter = Cc[
      "@mozilla.org/appshell/component/browser-status-filter;1"
    ].createInstance(Ci.nsIWebProgress);

    filter.addProgressListener(progressListener, Ci.nsIWebProgress.NOTIFY_ALL);

    this.filtersMap.set(docShell, filter);

    let webProgress = docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(filter, Ci.nsIWebProgress.NOTIFY_ALL);
  }

  disable(docShell) {
    let filter = this.filtersMap.get(docShell);
    if (!filter) {
      return;
    }

    let webProgress = docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.removeProgressListener(filter);

    filter.removeProgressListener(progressListener);
    this.filtersMap.delete(docShell);
  }
}
