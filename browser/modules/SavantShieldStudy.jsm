/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint semi: error */

"use strict";

var EXPORTED_SYMBOLS = ["SavantShieldStudy"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm"
});

// See LOG_LEVELS in Console.jsm. Examples: "all", "info", "warn", & "error".
const PREF_LOG_LEVEL = "shield.savant.loglevel";

// Create a new instance of the ConsoleAPI so we can control the maxLogLevel with a pref.
XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = ChromeUtils.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
  let consoleOptions = {
    maxLogLevelPref: PREF_LOG_LEVEL,
    prefix: "SavantShieldStudy",
  };
  return new ConsoleAPI(consoleOptions);
});

class SavantShieldStudyClass {
  constructor() {
    this.STUDY_PREF = "shield.savant.enabled";
    this.STUDY_TELEMETRY_CATEGORY = "savant";
    this.ALWAYS_PRIVATE_BROWSING_PREF = "browser.privatebrowsing.autostart";
    this.STUDY_DURATION_OVERRIDE_PREF = "shield.savant.duration_override";
    this.STUDY_EXPIRATION_DATE_PREF = "shield.savant.expiration_date";
    // ms = 'x' weeks * 7 days/week * 24 hours/day * 60 minutes/hour
    // * 60 seconds/minute * 1000 milliseconds/second
    this.DEFAULT_STUDY_DURATION_MS = 4 * 7 * 24 * 60 * 60 * 1000;
    // If on startupStudy(), user is ineligible or study has expired,
    // no probe listeners from this module have been added yet
    this.shouldRemoveListeners = true;
  }

  init() {
    this.telemetryEvents = new TelemetryEvents(this.STUDY_TELEMETRY_CATEGORY);
    this.addonListener = new AddonListener(this.STUDY_TELEMETRY_CATEGORY);
    this.bookmarkObserver = new BookmarkObserver(this.STUDY_TELEMETRY_CATEGORY);
    this.menuListener = new MenuListener(this.STUDY_TELEMETRY_CATEGORY);

    // check the pref in case Normandy flipped it on before we could add the pref listener
    this.shouldCollect = Services.prefs.getBoolPref(this.STUDY_PREF);
    if (this.shouldCollect) {
      this.startupStudy();
    }
    Services.prefs.addObserver(this.STUDY_PREF, this);
  }

  observe(subject, topic, data) {
    if (topic === "nsPref:changed" && data === this.STUDY_PREF) {
      // toggle state of the pref
      this.shouldCollect = !this.shouldCollect;
      if (this.shouldCollect) {
        this.startupStudy();
      } else {
        // The pref has been turned off
        this.endStudy("study_disable");
      }
    }
  }

  startupStudy() {
    // enable before any possible calls to endStudy, since it sends an 'end_study' event
    this.telemetryEvents.enableCollection();

    if (!this.isEligible()) {
      this.shouldRemoveListeners = false;
      this.endStudy("ineligible");
      return;
    }

    this.initStudyDuration();

    if (this.isStudyExpired()) {
      log.debug("Study expired in between this and the previous session.");
      this.shouldRemoveListeners = false;
      this.endStudy("expired");
    }

    this.addonListener.init();
    this.bookmarkObserver.init();
    this.menuListener.init();
  }

  isEligible() {
    const isAlwaysPrivateBrowsing = Services.prefs.getBoolPref(this.ALWAYS_PRIVATE_BROWSING_PREF);
    if (isAlwaysPrivateBrowsing) {
      return false;
    }

    return true;
  }

  initStudyDuration() {
    if (Services.prefs.getStringPref(this.STUDY_EXPIRATION_DATE_PREF, "")) {
      return;
    }
    Services.prefs.setStringPref(
      this.STUDY_EXPIRATION_DATE_PREF,
      this.getExpirationDateString()
    );
  }

  getDurationFromPref() {
    return Services.prefs.getIntPref(this.STUDY_DURATION_OVERRIDE_PREF, 0);
  }

  getExpirationDateString() {
    const now = Date.now();
    const studyDurationInMs =
    this.getDurationFromPref()
      || this.DEFAULT_STUDY_DURATION_MS;
    const expirationDateInt = now + studyDurationInMs;
    return new Date(expirationDateInt).toISOString();
  }

  isStudyExpired() {
    const expirationDateInt =
      Date.parse(Services.prefs.getStringPref(
        this.STUDY_EXPIRATION_DATE_PREF,
        this.getExpirationDateString()
      ));

    if (isNaN(expirationDateInt)) {
      log.error(
        `The value for the preference ${this.STUDY_EXPIRATION_DATE_PREF} is invalid.`
      );
      return false;
    }

    if (Date.now() > expirationDateInt) {
      return true;
    }
    return false;
  }

  endStudy(reason) {
    log.debug(`Ending the study due to reason: ${ reason }`);
    const isStudyEnding = true;
    Services.telemetry.recordEvent(this.STUDY_TELEMETRY_CATEGORY, "end_study", reason, null,
                                  { subcategory: "shield" });
    this.telemetryEvents.disableCollection();
    this.uninit(isStudyEnding);
    // These prefs needs to persist between restarts, so only reset on endStudy
    Services.prefs.clearUserPref(this.STUDY_PREF);
    Services.prefs.clearUserPref(this.STUDY_EXPIRATION_DATE_PREF);
  }

  // Called on every Firefox shutdown and endStudy
  uninit(isStudyEnding = false) {
    // if just shutting down, check for expiration, so the endStudy event can
    // be sent along with this session's main ping.
    if (!isStudyEnding && this.isStudyExpired()) {
      log.debug("Study expired during this session.");
      this.endStudy("expired");
      return;
    }

    this.addonListener.uninit();
    this.bookmarkObserver.uninit();
    this.menuListener.uninit();

    Services.prefs.removeObserver(this.ALWAYS_PRIVATE_BROWSING_PREF, this);
    Services.prefs.removeObserver(this.STUDY_PREF, this);
    Services.prefs.removeObserver(this.STUDY_DURATION_OVERRIDE_PREF, this);
    Services.prefs.clearUserPref(PREF_LOG_LEVEL);
    Services.prefs.clearUserPref(this.STUDY_DURATION_OVERRIDE_PREF);
  }
}

// References:
// - https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/normandy/lib/TelemetryEvents.jsm
// - https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/normandy/lib/PreferenceExperiments.jsm#l357
class TelemetryEvents {
  constructor(studyCategory) {
    this.STUDY_TELEMETRY_CATEGORY = studyCategory;
  }

  enableCollection() {
    log.debug("Study has been enabled; turning ON data collection.");
    Services.telemetry.setEventRecordingEnabled(this.STUDY_TELEMETRY_CATEGORY, true);
  }

  disableCollection() {
    log.debug("Study has been disabled; turning OFF data collection.");
    Services.telemetry.setEventRecordingEnabled(this.STUDY_TELEMETRY_CATEGORY, false);
  }
}

class AddonListener {
  constructor(studyCategory) {
    this.STUDY_TELEMETRY_CATEGORY = studyCategory;
    this.METHOD = "addon";
    this.EXTRA_SUBCATEGORY = "customize";
  }

  init() {
    this.listener = {
      onInstalling: (addon, needsRestart) => {
        const addon_id = addon.id;
        this.recordEvent("install_start", addon_id);
      },

      onInstalled: (addon) => {
        const addon_id = addon.id;
        this.recordEvent("install_finish", addon_id);
      },

      onEnabled: (addon) => {
        const addon_id = addon.id;
        this.recordEvent("enable", addon_id);
      },

      onDisabled: (addon) => {
        const addon_id = addon.id;
        this.recordEvent("disable", addon_id);
      },

      onUninstalling: (addon, needsRestart) => {
        const addon_id = addon.id;
        this.recordEvent("remove_start", addon_id);
      },

      onUninstalled: (addon) => {
        const addon_id = addon.id;
        this.recordEvent("remove_finish", addon_id);
      }
    };
    this.addListeners();
  }

  addListeners() {
    AddonManager.addAddonListener(this.listener);
  }

  recordEvent(event, addon_id) {
    log.debug(`Addon ID: ${addon_id}; event: ${ event }`);
    Services.telemetry.recordEvent(this.STUDY_TELEMETRY_CATEGORY,
                                  this.METHOD,
                                  event,
                                  addon_id,
                                  { subcategory: this.EXTRA_SUBCATEGORY });
  }

  removeListeners() {
    AddonManager.removeAddonListener(this.listener);
  }

  uninit() {
    if (SavantShieldStudy.shouldRemoveListeners) {
      this.removeListeners();
    }
  }
}

class BookmarkObserver {
  constructor(studyCategory) {
    this.STUDY_TELEMETRY_CATEGORY = studyCategory;
    // there are two probes: bookmark and follow_bookmark
    this.METHOD_1 = "bookmark";
    this.EXTRA_SUBCATEGORY_1 = "feature";
    this.METHOD_2 = "follow_bookmark";
    this.EXTRA_SUBCATEGORY_2 = "navigation";
    this.TYPE_BOOKMARK = Ci.nsINavBookmarksService.TYPE_BOOKMARK;
    // Ignore "fake" bookmarks created for bookmark tags
    this.skipTags = true;
  }

  init() {
    this.addObservers();
  }

  addObservers() {
    PlacesUtils.bookmarks.addObserver(this);
  }

  onItemAdded(itemID, parentID, index, itemType, uri, title, dateAdded, guid, parentGUID, source) {
    this.handleItemAddRemove(itemType, uri, source, "save");
  }

  onItemRemoved(itemID, parentID, index, itemType, uri, guid, parentGUID, source) {
    this.handleItemAddRemove(itemType, uri, source, "remove");
  }

  handleItemAddRemove(itemType, uri, source, event) {
    /*
    * "place:query" uris are used to create containers like Most Visited or
    * Recently Bookmarked. These are added as default bookmarks.
    */
    if (itemType === this.TYPE_BOOKMARK && !uri.schemeIs("place")
      && source === PlacesUtils.bookmarks.SOURCE_DEFAULT) {
      const isBookmarkProbe = true;
      this.recordEvent(event, isBookmarkProbe);
    }
  }

  // This observer is only fired for TYPE_BOOKMARK items.
  onItemVisited(itemID, visitID, time, transitionType, uri, parentID, guid, parentGUID) {
    const isBookmarkProbe = false;
    this.recordEvent("open", isBookmarkProbe);
  }

  recordEvent(event, isBookmarkProbe) {
    const method = isBookmarkProbe ? this.METHOD_1 : this.METHOD_2;
    const subcategory = isBookmarkProbe ? this.EXTRA_SUBCATEGORY_1 : this.EXTRA_SUBCATEGORY_2;
    Services.telemetry.recordEvent(this.STUDY_TELEMETRY_CATEGORY, method, event, null,
                                  {
                                    subcategory
                                  });
  }

  removeObservers() {
    PlacesUtils.bookmarks.removeObserver(this);
  }

  uninit() {
    if (SavantShieldStudy.shouldRemoveListeners) {
      this.removeObservers();
    }
  }
}

class MenuListener {
  constructor(studyCategory) {
    this.STUDY_TELEMETRY_CATEGORY = studyCategory;
    this.NAVIGATOR_TOOLBOX_ID = "navigator-toolbox";
    this.OVERFLOW_PANEL_ID = "widget-overflow";
    this.LIBRARY_PANELVIEW_ID = "appMenu-libraryView";
    this.HAMBURGER_PANEL_ID = "appMenu-popup";
    this.DOTDOTDOT_PANEL_ID = "pageActionPanel";
    this.windowWatcher = new WindowWatcher();
  }

  init() {
    this.windowWatcher.init(this.loadIntoWindow.bind(this),
    this.unloadFromWindow.bind(this),
    this.onWindowError.bind(this));
  }

 loadIntoWindow(win) {
    this.addListeners(win);
  }

  unloadFromWindow(win) {
    this.removeListeners(win);
  }

  onWindowError(msg) {
    log.error(msg);
  }

  addListeners(win) {
    const doc = win.document;
    const navToolbox = doc.getElementById(this.NAVIGATOR_TOOLBOX_ID);
    const overflowPanel = doc.getElementById(this.OVERFLOW_PANEL_ID);
    const hamburgerPanel = doc.getElementById(this.HAMBURGER_PANEL_ID);
    const dotdotdotPanel = doc.getElementById(this.DOTDOTDOT_PANEL_ID);

    /*
    * the library menu "ViewShowing" event bubbles up on the navToolbox in its
    * default location. A separate listener is needed if it is moved to the
    * overflow panel via Hamburger > Customize
    */
    navToolbox.addEventListener("ViewShowing", this);
    overflowPanel.addEventListener("ViewShowing", this);
    hamburgerPanel.addEventListener("popupshown", this);
    dotdotdotPanel.addEventListener("popupshown", this);
  }

  handleEvent(evt) {
    switch (evt.type) {
      case "ViewShowing":
        if (evt.target.id === this.LIBRARY_PANELVIEW_ID) {
          log.debug("Library panel opened.");
          this.recordEvent("library_menu");
        }
        break;
      case "popupshown":
        switch (evt.target.id) {
          case this.HAMBURGER_PANEL_ID:
            log.debug("Hamburger panel opened.");
            this.recordEvent("hamburger_menu");
            break;
          case this.DOTDOTDOT_PANEL_ID:
            log.debug("Dotdotdot panel opened.");
            this.recordEvent("dotdotdot_menu");
            break;
          default:
            break;
        }
      break;
    }
  }

  recordEvent(method) {
    Services.telemetry.recordEvent(this.STUDY_TELEMETRY_CATEGORY, method, "open", null,
                                  { subcategory: "menu" });
  }

  removeListeners(win) {
    const doc = win.document;
    const navToolbox = doc.getElementById(this.NAVIGATOR_TOOLBOX_ID);
    const overflowPanel = doc.getElementById(this.OVERFLOW_PANEL_ID);
    const hamburgerPanel = doc.getElementById(this.HAMBURGER_PANEL_ID);
    const dotdotdotPanel = doc.getElementById(this.DOTDOTDOT_PANEL_ID);

    try {
      navToolbox.removeEventListener("ViewShowing", this);
      overflowPanel.removeEventListener("ViewShowing", this);
      hamburgerPanel.removeEventListener("popupshown", this);
      dotdotdotPanel.removeEventListener("popupshown", this);
    } catch (err) {
      // Firefox is shutting down; elements have already been removed.
    }
  }

  uninit() {
    if (SavantShieldStudy.shouldRemoveListeners) {
      this.windowWatcher.uninit();
    }
  }
}

/*
* The WindowWatcher is used to add/remove listeners from MenuListener
* to/from all windows.
*/
class WindowWatcher {
  constructor() {
    this._isActive = false;
    this._loadCallback = null;
    this._unloadCallback = null;
    this._errorCallback = null;
  }

  // It is expected that loadCallback, unloadCallback, and errorCallback are bound
  // to a `this` value.
  init(loadCallback, unloadCallback, errorCallback) {
    if (this._isActive) {
      errorCallback("Called init, but WindowWatcher was already running");
      return;
    }

    this._isActive = true;
    this._loadCallback = loadCallback;
    this._unloadCallback = unloadCallback;
    this._errorCallback = errorCallback;

    // Add loadCallback to existing windows
    const windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      const win = windows.getNext();
      try {
        this._loadCallback(win);
      } catch (ex) {
        this._errorCallback(`WindowWatcher code loading callback failed: ${ ex }`);
      }
    }

    // Add loadCallback to future windows
    // This will call the observe method on WindowWatcher
    Services.ww.registerNotification(this);
  }

  uninit() {
    if (!this._isActive) {
      this._errorCallback("Called uninit, but WindowWatcher was already uninited");
      return;
    }

    const windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      const win = windows.getNext();
      try {
        this._unloadCallback(win);
      } catch (ex) {
        this._errorCallback(`WindowWatcher code unloading callback failed: ${ ex }`);
      }
    }

    Services.ww.unregisterNotification(this);

    this._loadCallback = null;
    this._unloadCallback = null;
    this._errorCallback = null;
    this._isActive = false;
  }

  observe(win, topic) {
    switch (topic) {
      case "domwindowopened":
        this._onWindowOpened(win);
        break;
      case "domwindowclosed":
        this._onWindowClosed(win);
        break;
      default:
        break;
    }
  }

  _onWindowOpened(win) {
    win.addEventListener("load", this, { once: true });
  }

  // only one event type expected: "load"
  handleEvent(evt) {
    const win = evt.target.ownerGlobal;

    // make sure we only add window listeners to a DOMWindow (browser.xul)
    const winType = win.document.documentElement.getAttribute("windowtype");
    if (winType === "navigator:browser") {
      this._loadCallback(win);
    }
  }

  _onWindowClosed(win) {
    this._unloadCallback(win);
  }
}

const SavantShieldStudy = new SavantShieldStudyClass();
