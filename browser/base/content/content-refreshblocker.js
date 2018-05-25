/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

ChromeUtils.import("resource://gre/modules/Services.jsm");

var RefreshBlocker = {
  PREF: "accessibility.blockautorefresh",

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

  init() {
    if (Services.prefs.getBoolPref(this.PREF)) {
      this.enable();
    }

    Services.prefs.addObserver(this.PREF, this);
  },

  uninit() {
    if (Services.prefs.getBoolPref(this.PREF)) {
      this.disable();
    }

    Services.prefs.removeObserver(this.PREF, this);
  },

  observe(subject, topic, data) {
    if (topic == "nsPref:changed" && data == this.PREF) {
      if (Services.prefs.getBoolPref(this.PREF)) {
        this.enable();
      } else {
        this.disable();
      }
    }
  },

  enable() {
    this._filter = Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
                     .createInstance(Ci.nsIWebProgress);
    this._filter.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_ALL);
    this._filter.target = tabEventTarget;

    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this._filter, Ci.nsIWebProgress.NOTIFY_ALL);

    addMessageListener("RefreshBlocker:Refresh", this);
  },

  disable() {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.removeProgressListener(this._filter);

    this._filter.removeProgressListener(this);
    this._filter = null;

    removeMessageListener("RefreshBlocker:Refresh", this);
  },

  send(data) {
    sendAsyncMessage("RefreshBlocker:Blocked", data);
  },

  /**
   * Notices when the nsIWebProgress transitions to STATE_STOP for
   * the STATE_IS_WINDOW case, which will clear any mappings from
   * blockedWindows.
   */
  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
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
        this.send(data);
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
    let outerWindowID = win.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils)
                           .outerWindowID;

    let data = {
      URI: aURI.spec,
      delay: aDelay,
      sameURI: aSameURI,
      outerWindowID,
    };

    if (this.blockedWindows.has(win)) {
      // onLocationChange must have fired before, so we can tell the
      // parent to show the notification.
      this.send(data);
    } else {
      // onLocationChange hasn't fired yet, so stash the data in the
      // map so that onLocationChange can send it when it fires.
      this.blockedWindows.set(win, data);
    }

    return false;
  },

  receiveMessage(message) {
    let data = message.data;

    if (message.name == "RefreshBlocker:Refresh") {
      let win = Services.wm.getOuterWindowWithId(data.outerWindowID);
      let refreshURI = win.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDocShell)
                          .QueryInterface(Ci.nsIRefreshURI);

      let URI = Services.io.newURI(data.URI);

      refreshURI.forceRefreshURI(URI, null, data.delay, true);
    }
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIWebProgressListener2, Ci.nsIWebProgressListener, Ci.nsISupportsWeakReference]),
};

RefreshBlocker.init();

addEventListener("unload", () => {
  RefreshBlocker.uninit();
});
