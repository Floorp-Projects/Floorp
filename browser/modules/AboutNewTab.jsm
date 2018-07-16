/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [ "AboutNewTab" ];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  ActivityStream: "resource://activity-stream/lib/ActivityStream.jsm",
  RemotePages: "resource://gre/modules/RemotePageManager.jsm"
});

const BROWSER_READY_NOTIFICATION = "sessionstore-windows-restored";

var AboutNewTab = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]),

  // AboutNewTab

  pageListener: null,

  isOverridden: false,

  activityStream: null,

  /**
   * init - Initializes an instance of Activity Stream if one doesn't exist already
   *        and creates the instance of Remote Page Manager which Activity Stream
   *        uses for message passing.
   *
   * @param {obj} pageListener - Optional argument. An existing instance of RemotePages
   *                             which Activity Stream has previously made, and we
   *                             would like to re-use.
   */
  init(pageListener) {
    if (this.isOverridden) {
      return;
    }

    // Since `init` can be called via `reset` at a later time with an existing
    // pageListener, we want to only add the observer if we are initializing
    // without this pageListener argument. This means it was the first call to `init`
    if (!pageListener) {
      Services.obs.addObserver(this, BROWSER_READY_NOTIFICATION);
    }

    this.pageListener = pageListener || new RemotePages(["about:home", "about:newtab", "about:welcome"]);
  },

  /**
   * onBrowserReady - Continues the initialization of Activity Stream after browser is ready.
   */
  onBrowserReady() {
    if (this.activityStream && this.activityStream.initialized) {
       return;
    }

    this.activityStream = new ActivityStream();
    try {
      this.activityStream.init();
    } catch (e) {
      Cu.reportError(e);
    }
  },

  /**
   * uninit - Uninitializes Activity Stream if it exists, and destroys the pageListener
   *        if it exists.
   */
  uninit() {
    if (this.activityStream) {
      this.activityStream.uninit();
      this.activityStream = null;
    }

    if (this.pageListener) {
      this.pageListener.destroy();
      this.pageListener = null;
    }
  },

  override(shouldPassPageListener) {
    this.isOverridden = true;
    const pageListener = this.pageListener;
    if (!pageListener)
      return null;
    if (shouldPassPageListener) {
      this.pageListener = null;
      return pageListener;
    }
    this.uninit();
    return null;
  },

  reset(pageListener) {
    this.isOverridden = false;
    this.init(pageListener);
  },

  // nsIObserver implementation

  observe(subject, topic, data) {
    switch (topic) {
      case BROWSER_READY_NOTIFICATION:
        Services.obs.removeObserver(this, BROWSER_READY_NOTIFICATION);
        // Avoid running synchronously during this event that's used for timing
        Services.tm.dispatchToMainThread(() => this.onBrowserReady());
        break;
    }
  }
};
