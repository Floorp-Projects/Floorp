/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ActivityStream: "resource://activity-stream/lib/ActivityStream.jsm",
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
  RemotePages:
    "resource://gre/modules/remotepagemanager/RemotePageManagerParent.jsm",
});

const ABOUT_URL = "about:newtab";
const PREF_ACTIVITY_STREAM_DEBUG = "browser.newtabpage.activity-stream.debug";
const TOPIC_APP_QUIT = "quit-application-granted";
const BROWSER_READY_NOTIFICATION = "sessionstore-windows-restored";

const AboutNewTab = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),

  // AboutNewTab
  initialized: false,

  pageListener: null,
  isPageListenerOverridden: false,
  willNotifyUser: false,

  _activityStreamEnabled: false,
  activityStream: null,
  activityStreamDebug: false,

  _cachedTopSites: null,

  _newTabURL: ABOUT_URL,
  _newTabURLOverridden: false,

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
    Services.obs.addObserver(this, TOPIC_APP_QUIT);
    if (!AppConstants.RELEASE_OR_BETA) {
      XPCOMUtils.defineLazyPreferenceGetter(
        this,
        "activityStreamDebug",
        PREF_ACTIVITY_STREAM_DEBUG,
        false,
        () => {
          this.notifyChange();
        }
      );
    }

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "privilegedAboutProcessEnabled",
      "browser.tabs.remote.separatePrivilegedContentProcess",
      false,
      () => {
        this.notifyChange();
      }
    );

    // More initialization happens here
    this.toggleActivityStream(true);
    this.initialized = true;

    if (this.isPageListenerOverridden) {
      return;
    }

    // Since `init` can be called via `reset` at a later time with an existing
    // pageListener, we want to only add the observer if we are initializing
    // without this pageListener argument. This means it was the first call to `init`
    if (!pageListener) {
      Services.obs.addObserver(this, BROWSER_READY_NOTIFICATION);
    }

    this.pageListener =
      pageListener ||
      new lazy.RemotePages(["about:home", "about:newtab", "about:welcome"]);
  },

  /**
   * React to changes to the activity stream being enabled or not.
   *
   * This will only act if there is a change of state and if not overridden.
   *
   * @returns {Boolean} Returns if there has been a state change
   *
   * @param {Boolean}   stateEnabled    activity stream enabled state to set to
   * @param {Boolean}   forceState      force state change
   */
  toggleActivityStream(stateEnabled, forceState = false) {
    if (
      !forceState &&
      (this._newTabURLOverridden ||
        stateEnabled === this._activityStreamEnabled)
    ) {
      // exit there is no change of state
      return false;
    }
    if (stateEnabled) {
      this._activityStreamEnabled = true;
    } else {
      this._activityStreamEnabled = false;
    }

    this._newTabURL = ABOUT_URL;
    return true;
  },

  get newTabURL() {
    return this._newTabURL;
  },

  set newTabURL(aNewTabURL) {
    let newTabURL = aNewTabURL.trim();
    if (newTabURL === ABOUT_URL) {
      // avoid infinite redirects in case one sets the URL to about:newtab
      this.resetNewTabURL();
      return;
    } else if (newTabURL === "") {
      newTabURL = "about:blank";
    }

    this.toggleActivityStream(false);
    this._newTabURL = newTabURL;
    this._newTabURLOverridden = true;
    this.notifyChange();
  },

  get newTabURLOverridden() {
    return this._newTabURLOverridden;
  },

  get activityStreamEnabled() {
    return this._activityStreamEnabled;
  },

  resetNewTabURL() {
    this._newTabURLOverridden = false;
    this._newTabURL = ABOUT_URL;
    this.toggleActivityStream(true, true);
    this.notifyChange();
  },

  notifyChange() {
    Services.obs.notifyObservers(null, "newtab-url-changed", this._newTabURL);
  },

  /**
   * onBrowserReady - Continues the initialization of Activity Stream after browser is ready.
   */
  onBrowserReady() {
    if (this.activityStream && this.activityStream.initialized) {
      return;
    }

    this.activityStream = new lazy.ActivityStream();
    try {
      this.activityStream.init();
      this._subscribeToActivityStream();
    } catch (e) {
      Cu.reportError(e);
    }
  },

  _subscribeToActivityStream() {
    let unsubscribe = this.activityStream.store.subscribe(() => {
      // If the top sites changed, broadcast "newtab-top-sites-changed". We
      // ignore changes to the `screenshot` property in each site because
      // screenshots are generated at times that are hard to predict and it ends
      // up interfering with tests that rely on "newtab-top-sites-changed".
      // Observers likely don't care about screenshots anyway.
      let topSites = this.activityStream.store
        .getState()
        .TopSites.rows.map(site => {
          site = { ...site };
          delete site.screenshot;
          return site;
        });
      if (!lazy.ObjectUtils.deepEqual(topSites, this._cachedTopSites)) {
        this._cachedTopSites = topSites;
        Services.obs.notifyObservers(null, "newtab-top-sites-changed");
      }
    });
    this._unsubscribeFromActivityStream = () => {
      try {
        unsubscribe();
      } catch (e) {
        Cu.reportError(e);
      }
    };
  },

  /**
   * uninit - Uninitializes Activity Stream if it exists, and destroys the pageListener
   *        if it exists.
   */
  uninit() {
    if (this.activityStream) {
      this._unsubscribeFromActivityStream?.();
      this.activityStream.uninit();
      this.activityStream = null;
    }

    if (this.pageListener) {
      this.pageListener.destroy();
      this.pageListener = null;
    }
    this.initialized = false;
  },

  overridePageListener(shouldPassPageListener) {
    this.isPageListenerOverridden = true;

    const pageListener = this.pageListener;
    if (!pageListener) {
      return null;
    }
    if (shouldPassPageListener) {
      this.pageListener = null;
      return pageListener;
    }
    this.uninit();
    return null;
  },

  reset(pageListener) {
    this.isPageListenerOverridden = false;
    this.init(pageListener);
  },

  getTopSites() {
    return this.activityStream
      ? this.activityStream.store.getState().TopSites.rows
      : [];
  },

  _alreadyRecordedTopsitesPainted: false,
  _nonDefaultStartup: false,

  noteNonDefaultStartup() {
    this._nonDefaultStartup = true;
  },

  maybeRecordTopsitesPainted(timestamp) {
    if (this._alreadyRecordedTopsitesPainted || this._nonDefaultStartup) {
      return;
    }

    const SCALAR_KEY = "timestamps.about_home_topsites_first_paint";

    let startupInfo = Services.startup.getStartupInfo();
    let processStartTs = startupInfo.process.getTime();
    let delta = Math.round(timestamp - processStartTs);
    Services.telemetry.scalarSet(SCALAR_KEY, delta);
    ChromeUtils.addProfilerMarker("aboutHomeTopsitesFirstPaint");
    this._alreadyRecordedTopsitesPainted = true;
  },

  // nsIObserver implementation

  observe(subject, topic, data) {
    switch (topic) {
      case TOPIC_APP_QUIT: {
        // We defer to this to the next tick of the event loop since the
        // AboutHomeStartupCache might want to read from the ActivityStream
        // store during TOPIC_APP_QUIT.
        Services.tm.dispatchToMainThread(() => this.uninit());
        break;
      }
      case BROWSER_READY_NOTIFICATION: {
        Services.obs.removeObserver(this, BROWSER_READY_NOTIFICATION);
        // Avoid running synchronously during this event that's used for timing
        Services.tm.dispatchToMainThread(() => this.onBrowserReady());
        break;
      }
    }
  },
};

var EXPORTED_SYMBOLS = ["AboutNewTab"];
