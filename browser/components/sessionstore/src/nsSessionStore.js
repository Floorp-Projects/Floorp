/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the nsSessionStore component.
 *
 * The Initial Developer of the Original Code is
 * Simon Bünzli <zeniko@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dietrich Ayala <dietrich@mozilla.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Session Storage and Restoration
 * 
 * Overview
 * This service keeps track of a user's session, storing the various bits
 * required to return the browser to its current state. The relevant data is 
 * stored in memory, and is periodically saved to disk in a file in the 
 * profile directory. The service is started at first window load, in
 * delayedStartup, and will restore the session from the data received from
 * the nsSessionStartup service.
 */

/* :::::::: Constants and Helpers ::::::::::::::: */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

const STATE_STOPPED = 0;
const STATE_RUNNING = 1;
const STATE_QUITTING = -1;

const STATE_STOPPED_STR = "stopped";
const STATE_RUNNING_STR = "running";

const PRIVACY_NONE = 0;
const PRIVACY_ENCRYPTED = 1;
const PRIVACY_FULL = 2;

const NOTIFY_WINDOWS_RESTORED = "sessionstore-windows-restored";

// global notifications observed
const OBSERVING = [
  "domwindowopened", "domwindowclosed",
  "quit-application-requested", "quit-application-granted",
  "quit-application", "browser:purge-session-history",
  "private-browsing"
];

/*
XUL Window properties to (re)store
Restored in restoreDimensions()
*/
const WINDOW_ATTRIBUTES = ["width", "height", "screenX", "screenY", "sizemode"];

/* 
Hideable window features to (re)store
Restored in restoreWindowFeatures()
*/
const WINDOW_HIDEABLE_FEATURES = [
  "menubar", "toolbar", "locationbar", 
  "personalbar", "statusbar", "scrollbars"
];

/*
docShell capabilities to (re)store
Restored in restoreHistory()
eg: browser.docShell["allow" + aCapability] = false;
*/
const CAPABILITIES = [
  "Subframes", "Plugins", "Javascript", "MetaRedirects", "Images"
];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function debug(aMsg) {
  aMsg = ("SessionStore: " + aMsg).replace(/\S{80}/g, "$&\n");
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService)
                                     .logStringMessage(aMsg);
}

/* :::::::: The Service ::::::::::::::: */

function SessionStoreService() {
}

SessionStoreService.prototype = {
  classDescription: "Browser Session Store Service",
  contractID: "@mozilla.org/browser/sessionstore;1",
  classID: Components.ID("{5280606b-2510-4fe0-97ef-9b5a22eafe6b}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISessionStore,
                                         Ci.nsIDOMEventListener,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  // xul:tab attributes to (re)store (extensions might want to hook in here);
  // the favicon is always saved for the about:sessionrestore page
  xulAttributes: ["image"],

  // set default load state
  _loadState: STATE_STOPPED,

  // minimal interval between two save operations (in milliseconds)
  _interval: 10000,

  // when crash recovery is disabled, session data is not written to disk
  _resume_from_crash: true,

  // During the initial restore tracks the number of windows yet to be restored
  _restoreCount: 0,

  // time in milliseconds (Date.now()) when the session was last written to file
  _lastSaveTime: 0, 

  // states for all currently opened windows
  _windows: {},

  // in case the last closed window ain't a navigator:browser one
  // (also contains browser popup windows closed after the last non-popup one)
  _lastClosedWindows: null,

  // not-"dirty" windows usually don't need to have their data updated
  _dirtyWindows: {},

  // collection of session states yet to be restored
  _statesToRestore: {},

  // counts the number of crashes since the last clean start
  _recentCrashes: 0,

  // whether we are in private browsing mode
  _inPrivateBrowsing: false,

/* ........ Global Event Handlers .............. */

  /**
   * Initialize the component
   */
  init: function sss_init(aWindow) {
    if (!aWindow || this._loadState == STATE_RUNNING) {
      // make sure that all browser windows which try to initialize
      // SessionStore are really tracked by it
      if (aWindow && (!aWindow.__SSi || !this._windows[aWindow.__SSi]))
        this.onLoad(aWindow);
      return;
    }

    this._prefBranch = Cc["@mozilla.org/preferences-service;1"].
                       getService(Ci.nsIPrefService).getBranch("browser.");
    this._prefBranch.QueryInterface(Ci.nsIPrefBranch2);

    var observerService = Cc["@mozilla.org/observer-service;1"].
                          getService(Ci.nsIObserverService);

    OBSERVING.forEach(function(aTopic) {
      observerService.addObserver(this, aTopic, true);
    }, this);

    var pbs = Cc["@mozilla.org/privatebrowsing;1"].
              getService(Ci.nsIPrivateBrowsingService);
    this._inPrivateBrowsing = pbs.privateBrowsingEnabled;

    // get interval from prefs - used often, so caching/observing instead of fetching on-demand
    this._interval = this._prefBranch.getIntPref("sessionstore.interval");
    this._prefBranch.addObserver("sessionstore.interval", this, true);
    
    // get crash recovery state from prefs and allow for proper reaction to state changes
    this._resume_from_crash = this._prefBranch.getBoolPref("sessionstore.resume_from_crash");
    this._prefBranch.addObserver("sessionstore.resume_from_crash", this, true);
    
    // observe prefs changes so we can modify stored data to match
    this._prefBranch.addObserver("sessionstore.max_tabs_undo", this, true);

    // get file references
    var dirService = Cc["@mozilla.org/file/directory_service;1"].
                     getService(Ci.nsIProperties);
    this._sessionFile = dirService.get("ProfD", Ci.nsILocalFile);
    this._sessionFileBackup = this._sessionFile.clone();
    this._sessionFile.append("sessionstore.js");
    this._sessionFileBackup.append("sessionstore.bak");

    // get string containing session state
    var iniString;
    try {
      var ss = Cc["@mozilla.org/browser/sessionstartup;1"].
               getService(Ci.nsISessionStartup);
      if (ss.doRestore())
        iniString = ss.state;
    }
    catch(ex) { dump(ex + "\n"); } // no state to restore, which is ok

    if (iniString) {
      try {
        // parse the session state into JS objects
        this._initialState = this._safeEval(iniString);
        
        let lastSessionCrashed =
          this._initialState.session && this._initialState.session.state &&
          this._initialState.session.state == STATE_RUNNING_STR;
        if (lastSessionCrashed) {
          this._recentCrashes = (this._initialState.session &&
                                 this._initialState.session.recentCrashes || 0) + 1;
          
          if (this._needsRestorePage(this._initialState, this._recentCrashes)) {
            // replace the crashed session with a restore-page-only session
            let pageData = {
              url: "about:sessionrestore",
              formdata: { "#sessionData": iniString }
            };
            this._initialState = { windows: [{ tabs: [{ entries: [pageData] }] }] };
          }
        }
        
        // make sure that at least the first window doesn't have anything hidden
        delete this._initialState.windows[0].hidden;
      }
      catch (ex) { debug("The session file is invalid: " + ex); }
    }

    // remove the session data files if crash recovery is disabled
    if (!this._resume_from_crash)
      this._clearDisk();
    else { // create a backup if the session data file exists
      try {
        if (this._sessionFileBackup.exists())
          this._sessionFileBackup.remove(false);
        if (this._sessionFile.exists())
          this._sessionFile.copyTo(null, this._sessionFileBackup.leafName);
      }
      catch (ex) { Cu.reportError(ex); } // file was write-locked?
    }

    // at this point, we've as good as resumed the session, so we can
    // clear the resume_session_once flag, if it's set
    if (this._loadState != STATE_QUITTING &&
        this._prefBranch.getBoolPref("sessionstore.resume_session_once"))
      this._prefBranch.setBoolPref("sessionstore.resume_session_once", false);
    
    // As this is called at delayedStartup, restoration must be initiated here
    this.onLoad(aWindow);
  },

  /**
   * Called on application shutdown, after notifications:
   * quit-application-granted, quit-application
   */
  _uninit: function sss_uninit() {
    if (this._doResumeSession()) { // save all data for session resuming 
      this.saveState(true);
    }
    else { // discard all session related data 
      this._clearDisk();
    }
    // Make sure to break our cycle with the save timer
    if (this._saveTimer) {
      this._saveTimer.cancel();
      this._saveTimer = null;
    }
  },

  /**
   * Handle notifications
   */
  observe: function sss_observe(aSubject, aTopic, aData) {
    // for event listeners
    var _this = this;

    switch (aTopic) {
    case "domwindowopened": // catch new windows
      aSubject.addEventListener("load", function(aEvent) {
        aEvent.currentTarget.removeEventListener("load", arguments.callee, false);
        _this.onLoad(aEvent.currentTarget);
        }, false);
      break;
    case "domwindowclosed": // catch closed windows
      this.onClose(aSubject);
      break;
    case "quit-application-requested":
      // get a current snapshot of all windows
      this._forEachBrowserWindow(function(aWindow) {
        this._collectWindowData(aWindow);
      });
      this._dirtyWindows = [];
      break;
    case "quit-application-granted":
      // freeze the data at what we've got (ignoring closing windows)
      this._loadState = STATE_QUITTING;
      break;
    case "quit-application":
      if (aData == "restart")
        this._prefBranch.setBoolPref("sessionstore.resume_session_once", true);
      this._loadState = STATE_QUITTING; // just to be sure
      this._uninit();
      break;
    case "browser:purge-session-history": // catch sanitization 
      let openWindows = {};
      this._forEachBrowserWindow(function(aWindow) {
        Array.forEach(aWindow.getBrowser().browsers, function(aBrowser) {
          delete aBrowser.parentNode.__SS_data;
        });
        openWindows[aWindow.__SSi] = true;
      });
      // also clear all data about closed tabs and windows
      for (ix in this._windows) {
        if (ix in openWindows)
          this._windows[ix]._closedTabs = [];
        else
          delete this._windows[ix];
      }
      this._lastClosedWindows = null;
      this._clearDisk();
      // give the tabbrowsers a chance to clear their histories first
      var win = this._getMostRecentBrowserWindow();
      if (win)
        win.setTimeout(function() { _this.saveState(true); }, 0);
      else if (this._loadState == STATE_RUNNING)
        this.saveState(true);
      // Delete the private browsing backed up state, if any
      if ("_stateBackup" in this)
        delete this._stateBackup;
      break;
    case "nsPref:changed": // catch pref changes
      switch (aData) {
      // if the user decreases the max number of closed tabs they want
      // preserved update our internal states to match that max
      case "sessionstore.max_tabs_undo":
        var ix;
        for (ix in this._windows) {
          this._windows[ix]._closedTabs.splice(this._prefBranch.getIntPref("sessionstore.max_tabs_undo"));
        }
        break;
      case "sessionstore.interval":
        this._interval = this._prefBranch.getIntPref("sessionstore.interval");
        // reset timer and save
        if (this._saveTimer) {
          this._saveTimer.cancel();
          this._saveTimer = null;
        }
        this.saveStateDelayed(null, -1);
        break;
      case "sessionstore.resume_from_crash":
        this._resume_from_crash = this._prefBranch.getBoolPref("sessionstore.resume_from_crash");
        // either create the file with crash recovery information or remove it
        // (when _loadState is not STATE_RUNNING, that file is used for session resuming instead)
        if (this._resume_from_crash)
          this.saveState(true);
        else if (this._loadState == STATE_RUNNING)
          this._clearDisk();
        break;
      }
      break;
    case "timer-callback": // timer call back for delayed saving
      this._saveTimer = null;
      this.saveState();
      break;
    case "private-browsing":
      switch (aData) {
      case "enter":
        this.saveState(true);
        this._inPrivateBrowsing = true;
        this._stateBackup = this._safeEval(this._getCurrentState(true).toSource());
        break;
      case "exit":
        aSubject.QueryInterface(Ci.nsISupportsPRBool);
        let quitting = aSubject.data;
        if (quitting) {
          // save the backed up state with session set to stopped,
          // otherwise resuming next time would look like a crash
          if ("_stateBackup" in this) {
            var oState = this._stateBackup;
            oState.session = { state: STATE_STOPPED_STR };

            this._saveStateObject(oState);
          }
          // make sure to restore the non-private session upon resuming
          this._prefBranch.setBoolPref("sessionstore.resume_session_once", true);
        }
        else
          this._inPrivateBrowsing = false;
        delete this._stateBackup;
        break;
      }
      break;
    }
  },

/* ........ Window Event Handlers .............. */

  /**
   * Implement nsIDOMEventListener for handling various window and tab events
   */
  handleEvent: function sss_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "load":
      case "pageshow":
        this.onTabLoad(aEvent.currentTarget.ownerDocument.defaultView, aEvent.currentTarget, aEvent);
        break;
      case "change":
      case "input":
      case "DOMAutoComplete":
        this.onTabInput(aEvent.currentTarget.ownerDocument.defaultView, aEvent.currentTarget);
        break;
      case "scroll":
        this.onTabScroll(aEvent.currentTarget.ownerDocument.defaultView);
        break;
      case "TabOpen":
      case "TabClose":
        var panelID = aEvent.originalTarget.linkedPanel;
        var tabpanel = aEvent.originalTarget.ownerDocument.getElementById(panelID);
        if (aEvent.type == "TabOpen") {
          this.onTabAdd(aEvent.currentTarget.ownerDocument.defaultView, tabpanel);
        }
        else {
          this.onTabClose(aEvent.currentTarget.ownerDocument.defaultView, aEvent.originalTarget);
          this.onTabRemove(aEvent.currentTarget.ownerDocument.defaultView, tabpanel);
        }
        break;
      case "TabSelect":
        var tabpanels = aEvent.currentTarget.mPanelContainer;
        this.onTabSelect(aEvent.currentTarget.ownerDocument.defaultView, tabpanels);
        break;
    }
  },

  /**
   * If it's the first window load since app start...
   * - determine if we're reloading after a crash or a forced-restart
   * - restore window state
   * - restart downloads
   * Set up event listeners for this window's tabs
   * @param aWindow
   *        Window reference
   */
  onLoad: function sss_onLoad(aWindow) {
    // return if window has already been initialized
    if (aWindow && aWindow.__SSi && this._windows[aWindow.__SSi])
      return;

    // ignore non-browser windows and windows opened while shutting down
    if (aWindow.document.documentElement.getAttribute("windowtype") != "navigator:browser" ||
      this._loadState == STATE_QUITTING)
      return;

    // assign it a unique identifier (timestamp)
    aWindow.__SSi = "window" + Date.now();

    // and create its data object
    this._windows[aWindow.__SSi] = { tabs: [], selected: 0, _closedTabs: [] };
    if (!aWindow.toolbar.visible)
      this._windows[aWindow.__SSi].isPopup = true;
    
    // perform additional initialization when the first window is loading
    if (this._loadState == STATE_STOPPED) {
      this._loadState = STATE_RUNNING;
      this._lastSaveTime = Date.now();
      
      // restore a crashed session resp. resume the last session if requested
      if (this._initialState) {
        // make sure that the restored tabs are first in the window
        this._initialState._firstTabs = true;
        this._restoreCount = this._initialState.windows ? this._initialState.windows.length : 0;
        this.restoreWindow(aWindow, this._initialState, this._isCmdLineEmpty(aWindow));
        delete this._initialState;
      }
      else {
        // Nothing to restore, notify observers things are complete.
        var observerService = Cc["@mozilla.org/observer-service;1"].
                              getService(Ci.nsIObserverService);
        observerService.notifyObservers(null, NOTIFY_WINDOWS_RESTORED, "");
      }
      
      // mark ourselves as running
      this.saveState(true);
    }
    // this window was opened by _openWindowWithState
    else if (!this._isWindowLoaded(aWindow)) {
      let followUp = this._statesToRestore[aWindow.__SS_restoreID].windows.length == 1;
      this.restoreWindow(aWindow, this._statesToRestore[aWindow.__SS_restoreID], true, followUp);
    }
    
    var tabbrowser = aWindow.getBrowser();
    var tabpanels = tabbrowser.mPanelContainer;
    
    // add tab change listeners to all already existing tabs
    for (var i = 0; i < tabpanels.childNodes.length; i++) {
      this.onTabAdd(aWindow, tabpanels.childNodes[i], true);
    }
    // notification of tab add/remove/selection
    tabbrowser.addEventListener("TabOpen", this, true);
    tabbrowser.addEventListener("TabClose", this, true);
    tabbrowser.addEventListener("TabSelect", this, true);
  },

  /**
   * On window close...
   * - remove event listeners from tabs
   * - save all window data
   * @param aWindow
   *        Window reference
   */
  onClose: function sss_onClose(aWindow) {
    // this window was about to be restored - conserve its original data, if any
    let isFullyLoaded = this._isWindowLoaded(aWindow);
    if (!isFullyLoaded) {
      if (!aWindow.__SSi)
        aWindow.__SSi = "window" + Date.now();
      this._window[aWindow.__SSi] = this._statesToRestore[aWindow.__SS_restoreID];
      delete this._statesToRestore[aWindow.__SS_restoreID];
      delete aWindow.__SS_restoreID;
    }
    
    // ignore windows not tracked by SessionStore
    if (!aWindow.__SSi || !this._windows[aWindow.__SSi]) {
      return;
    }
    
    if (this.windowToFocus && this.windowToFocus == aWindow) {
      delete this.windowToFocus;
    }
    
    var tabbrowser = aWindow.getBrowser();
    var tabpanels = tabbrowser.mPanelContainer;

    tabbrowser.removeEventListener("TabOpen", this, true);
    tabbrowser.removeEventListener("TabClose", this, true);
    tabbrowser.removeEventListener("TabSelect", this, true);
    
    let winData = this._windows[aWindow.__SSi];
    if (this._loadState == STATE_RUNNING) { // window not closed during a regular shut-down 
      // update all window data for a last time
      this._collectWindowData(aWindow);
      
      // preserve this window's data (in case it was the last navigator:browser)
      // if this is a popup window, append it to what we've already got (cf. bug 368677)
      if (!this._lastClosedWindows || !winData.isPopup)
        this._lastClosedWindows = [winData];
      else
        this._lastClosedWindows.push(winData);
      
      if (isFullyLoaded) {
        winData.title = aWindow.content.document.title;
        this._updateCookies(this._lastClosedWindows);
      }
      
      // clear this window from the list
      delete this._windows[aWindow.__SSi];
      
      // save the state without this window to disk
      this.saveStateDelayed();
    }
    
    for (var i = 0; i < tabpanels.childNodes.length; i++) {
      this.onTabRemove(aWindow, tabpanels.childNodes[i], true);
    }
    
    // cache the window state until the window is completely gone
    aWindow.__SS_dyingCache = winData;
    
    delete aWindow.__SSi;
  },

  /**
   * set up listeners for a new tab
   * @param aWindow
   *        Window reference
   * @param aPanel
   *        TabPanel reference
   * @param aNoNotification
   *        bool Do not save state if we're updating an existing tab
   */
  onTabAdd: function sss_onTabAdd(aWindow, aPanel, aNoNotification) {
    aPanel.addEventListener("load", this, true);
    aPanel.addEventListener("pageshow", this, true);
    aPanel.addEventListener("change", this, true);
    aPanel.addEventListener("input", this, true);
    aPanel.addEventListener("DOMAutoComplete", this, true);
    aPanel.addEventListener("scroll", this, true);
    
    if (!aNoNotification) {
      this.saveStateDelayed(aWindow);
    }
  },

  /**
   * remove listeners for a tab
   * @param aWindow
   *        Window reference
   * @param aPanel
   *        TabPanel reference
   * @param aNoNotification
   *        bool Do not save state if we're updating an existing tab
   */
  onTabRemove: function sss_onTabRemove(aWindow, aPanel, aNoNotification) {
    aPanel.removeEventListener("load", this, true);
    aPanel.removeEventListener("pageshow", this, true);
    aPanel.removeEventListener("change", this, true);
    aPanel.removeEventListener("input", this, true);
    aPanel.removeEventListener("DOMAutoComplete", this, true);
    aPanel.removeEventListener("scroll", this, true);
    
    delete aPanel.__SS_data;
    
    if (!aNoNotification) {
      this.saveStateDelayed(aWindow);
    }
  },

  /**
   * When a tab closes, collect its properties
   * @param aWindow
   *        Window reference
   * @param aTab
   *        TabPanel reference
   */
  onTabClose: function sss_onTabClose(aWindow, aTab) {
    // notify the tabbrowser that the tab state will be retrieved for the last time
    // (so that extension authors can easily set data on soon-to-be-closed tabs)
    var event = aWindow.document.createEvent("Events");
    event.initEvent("SSTabClosing", true, false);
    aTab.dispatchEvent(event);
    
    var maxTabsUndo = this._prefBranch.getIntPref("sessionstore.max_tabs_undo");
    // don't update our internal state if we don't have to
    if (maxTabsUndo == 0) {
      return;
    }
    
    // make sure that the tab related data is up-to-date
    var tabState = this._collectTabData(aTab);
    this._updateTextAndScrollDataForTab(aWindow, aTab.linkedBrowser, tabState);

    // store closed-tab data for undo
    if (tabState.entries.length > 0) {
      let tabTitle = aTab.label;
      let tabbrowser = aWindow.gBrowser;
      // replace "Loading..." with the document title (with minimal side-effects)
      if (tabTitle == tabbrowser.mStringBundle.getString("tabs.loading")) {
        tabbrowser.setTabTitle(aTab);
        [tabTitle, aTab.label] = [aTab.label, tabTitle];
      }
      
      this._windows[aWindow.__SSi]._closedTabs.unshift({
        state: tabState,
        title: tabTitle,
        image: aTab.getAttribute("image"),
        pos: aTab._tPos
      });
      var length = this._windows[aWindow.__SSi]._closedTabs.length;
      if (length > maxTabsUndo)
        this._windows[aWindow.__SSi]._closedTabs.splice(maxTabsUndo, length - maxTabsUndo);
    }
  },

  /**
   * When a tab loads, save state.
   * @param aWindow
   *        Window reference
   * @param aPanel
   *        TabPanel reference
   * @param aEvent
   *        Event obj
   */
  onTabLoad: function sss_onTabLoad(aWindow, aPanel, aEvent) { 
    // react on "load" and solitary "pageshow" events (the first "pageshow"
    // following "load" is too late for deleting the data caches)
    if (aEvent.type != "load" && !aEvent.persisted) {
      return;
    }
    
    delete aPanel.__SS_data;
    this.saveStateDelayed(aWindow);
    
    // attempt to update the current URL we send in a crash report
    this._updateCrashReportURL(aWindow);
  },

  /**
   * Called when a tabpanel sends the "input" notification 
   * @param aWindow
   *        Window reference
   * @param aPanel
   *        TabPanel reference
   */
  onTabInput: function sss_onTabInput(aWindow, aPanel) {
    if (aPanel.__SS_data)
      delete aPanel.__SS_data._formDataSaved;
    
    this.saveStateDelayed(aWindow, 3000);
  },

  /**
   * Called when a tabpanel sends a "scroll" notification 
   * @param aWindow
   *        Window reference
   */
  onTabScroll: function sss_onTabScroll(aWindow) {
    this.saveStateDelayed(aWindow, 3000);
  },

  /**
   * When a tab is selected, save session data
   * @param aWindow
   *        Window reference
   * @param aPanels
   *        TabPanel reference
   */
  onTabSelect: function sss_onTabSelect(aWindow, aPanels) {
    if (this._loadState == STATE_RUNNING) {
      this._windows[aWindow.__SSi].selected = aPanels.selectedIndex;
      this.saveStateDelayed(aWindow);

      // attempt to update the current URL we send in a crash report
      this._updateCrashReportURL(aWindow);
    }
  },

/* ........ nsISessionStore API .............. */

  getBrowserState: function sss_getBrowserState() {
    return this._toJSONString(this._getCurrentState());
  },

  setBrowserState: function sss_setBrowserState(aState) {
    try {
      var state = this._safeEval("(" + aState + ")");
    }
    catch (ex) { /* invalid state object - don't restore anything */ }
    if (!state || !state.windows)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    
    var window = this._getMostRecentBrowserWindow();
    if (!window) {
      this._openWindowWithState(state);
      return;
    }

    // close all other browser windows
    this._forEachBrowserWindow(function(aWindow) {
      if (aWindow != window) {
        aWindow.close();
      }
    });

    // restore to the given state
    this.restoreWindow(window, state, true);
  },

  getWindowState: function sss_getWindowState(aWindow) {
    if (!aWindow.__SSi && !aWindow.__SS_dyingCache)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    
    if (!aWindow.__SSi)
      return this._toJSONString({ windows: [aWindow.__SS_dyingCache] });
    return this._toJSONString(this._getWindowState(aWindow));
  },

  setWindowState: function sss_setWindowState(aWindow, aState, aOverwrite) {
    if (!aWindow.__SSi)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    
    this.restoreWindow(aWindow, "(" + aState + ")", aOverwrite);
  },

  getTabState: function sss_getTabState(aTab) {
    if (!aTab.ownerDocument || !aTab.ownerDocument.defaultView.__SSi)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    
    var tabState = this._collectTabData(aTab);
    
    var window = aTab.ownerDocument.defaultView;
    this._updateTextAndScrollDataForTab(window, aTab.linkedBrowser, tabState);
    
    return this._toJSONString(tabState);
  },

  setTabState: function sss_setTabState(aTab, aState) {
    var tabState = this._safeEval("(" + aState + ")");
    if (!tabState.entries || !aTab.ownerDocument || !aTab.ownerDocument.defaultView.__SSi)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    
    var window = aTab.ownerDocument.defaultView;
    this.restoreHistoryPrecursor(window, [aTab], [tabState], 0, 0, 0);
  },

  duplicateTab: function sss_duplicateTab(aWindow, aTab) {
    if (!aTab.ownerDocument || !aTab.ownerDocument.defaultView.__SSi ||
        !aWindow.getBrowser)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    
    var tabState = this._collectTabData(aTab, true);
    var sourceWindow = aTab.ownerDocument.defaultView;
    this._updateTextAndScrollDataForTab(sourceWindow, aTab.linkedBrowser, tabState, true);
    
    var newTab = aWindow.getBrowser().addTab();
    this.restoreHistoryPrecursor(aWindow, [newTab], [tabState], 0, 0, 0);
    
    return newTab;
  },

  getClosedTabCount: function sss_getClosedTabCount(aWindow) {
    if (!aWindow.__SSi && aWindow.__SS_dyingCache)
      return aWindow.__SS_dyingCache._closedTabs.length;
    if (!aWindow.__SSi)
      // XXXzeniko shouldn't we throw here?
      return 0; // not a browser window, or not otherwise tracked by SS.
    
    return this._windows[aWindow.__SSi]._closedTabs.length;
  },

  getClosedTabData: function sss_getClosedTabDataAt(aWindow) {
    if (!aWindow.__SSi && !aWindow.__SS_dyingCache)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    
    if (!aWindow.__SSi)
      return this._toJSONString(aWindow.__SS_dyingCache._closedTabs);
    return this._toJSONString(this._windows[aWindow.__SSi]._closedTabs);
  },

  undoCloseTab: function sss_undoCloseTab(aWindow, aIndex) {
    if (!aWindow.__SSi)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    
    var closedTabs = this._windows[aWindow.__SSi]._closedTabs;

    // default to the most-recently closed tab
    aIndex = aIndex || 0;
    if (!(aIndex in closedTabs))
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    
    // fetch the data of closed tab, while removing it from the array
    let closedTab = closedTabs.splice(aIndex, 1).shift();
    let closedTabState = closedTab.state;

    // create a new tab
    let browser = aWindow.gBrowser;
    let tab = browser.addTab();
      
    // restore the tab's position
    browser.moveTabTo(tab, closedTab.pos);

    // restore tab content
    this.restoreHistoryPrecursor(aWindow, [tab], [closedTabState], 1, 0, 0);

    // focus the tab's content area
    let content = browser.getBrowserForTab(tab).contentWindow;
    aWindow.setTimeout(function() { content.focus(); }, 0);
    
    return tab;
  },

  getWindowValue: function sss_getWindowValue(aWindow, aKey) {
    if (aWindow.__SSi) {
      var data = this._windows[aWindow.__SSi].extData || {};
      return data[aKey] || "";
    }
    if (aWindow.__SS_dyingCache) {
      data = aWindow.__SS_dyingCache.extData || {};
      return data[aKey] || "";
    }
    throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
  },

  setWindowValue: function sss_setWindowValue(aWindow, aKey, aStringValue) {
    if (aWindow.__SSi) {
      if (!this._windows[aWindow.__SSi].extData) {
        this._windows[aWindow.__SSi].extData = {};
      }
      this._windows[aWindow.__SSi].extData[aKey] = aStringValue;
      this.saveStateDelayed(aWindow);
    }
    else {
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    }
  },

  deleteWindowValue: function sss_deleteWindowValue(aWindow, aKey) {
    if (aWindow.__SSi && this._windows[aWindow.__SSi].extData &&
        this._windows[aWindow.__SSi].extData[aKey])
      delete this._windows[aWindow.__SSi].extData[aKey];
    else
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
  },

  getTabValue: function sss_getTabValue(aTab, aKey) {
    var data = aTab.__SS_extdata || {};
    return data[aKey] || "";
  },

  setTabValue: function sss_setTabValue(aTab, aKey, aStringValue) {
    if (!aTab.__SS_extdata) {
      aTab.__SS_extdata = {};
    }
    aTab.__SS_extdata[aKey] = aStringValue;
    this.saveStateDelayed(aTab.ownerDocument.defaultView);
  },

  deleteTabValue: function sss_deleteTabValue(aTab, aKey) {
    if (aTab.__SS_extdata && aTab.__SS_extdata[aKey])
      delete aTab.__SS_extdata[aKey];
    else
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
  },

  persistTabAttribute: function sss_persistTabAttribute(aName) {
    if (this.xulAttributes.indexOf(aName) != -1)
      return; // this attribute is already being tracked
    
    this.xulAttributes.push(aName);
    this.saveStateDelayed();
  },

/* ........ Saving Functionality .............. */

  /**
   * Store all session data for a window
   * @param aWindow
   *        Window reference
   */
  _saveWindowHistory: function sss_saveWindowHistory(aWindow) {
    var tabbrowser = aWindow.getBrowser();
    var tabs = tabbrowser.mTabs;
    var tabsData = this._windows[aWindow.__SSi].tabs = [];
    
    for (var i = 0; i < tabs.length; i++)
      tabsData.push(this._collectTabData(tabs[i]));
    
    this._windows[aWindow.__SSi].selected = tabbrowser.mTabBox.selectedIndex + 1;
  },

  /**
   * Collect data related to a single tab
   * @param aTab
   *        tabbrowser tab
   * @param aFullData
   *        always return privacy sensitive data (use with care)
   * @returns object
   */
  _collectTabData: function sss_collectTabData(aTab, aFullData) {
    var tabData = { entries: [] };
    var browser = aTab.linkedBrowser;
    
    if (!browser || !browser.currentURI)
      // can happen when calling this function right after .addTab()
      return tabData;
    else if (browser.parentNode.__SS_data && browser.parentNode.__SS_data._tabStillLoading)
      // use the data to be restored when the tab hasn't been completely loaded
      return browser.parentNode.__SS_data;
    
    var history = null;
    try {
      history = browser.sessionHistory;
    }
    catch (ex) { } // this could happen if we catch a tab during (de)initialization
    
    if (history && browser.parentNode.__SS_data &&
        browser.parentNode.__SS_data.entries[history.index] && !aFullData) {
      tabData = browser.parentNode.__SS_data;
      tabData.index = history.index + 1;
    }
    else if (history && history.count > 0) {
      for (var j = 0; j < history.count; j++)
        tabData.entries.push(this._serializeHistoryEntry(history.getEntryAtIndex(j, false),
                                                         aFullData));
      tabData.index = history.index + 1;

      // make sure not to cache privacy sensitive data which shouldn't get out
      if (!aFullData)
        browser.parentNode.__SS_data = tabData;
    }
    else if (browser.currentURI.spec != "about:blank" ||
             browser.contentDocument.body.hasChildNodes()) {
      tabData.entries[0] = { url: browser.currentURI.spec };
      tabData.index = 1;
    }
    
    var disallow = [];
    for (var i = 0; i < CAPABILITIES.length; i++)
      if (!browser.docShell["allow" + CAPABILITIES[i]])
        disallow.push(CAPABILITIES[i]);
    if (disallow.length > 0)
      tabData.disallow = disallow.join(",");
    else if (tabData.disallow)
      delete tabData.disallow;
    
    if (this.xulAttributes.length > 0) {
      tabData.attributes = {};
      Array.forEach(aTab.attributes, function(aAttr) {
        if (this.xulAttributes.indexOf(aAttr.name) > -1)
          tabData.attributes[aAttr.name] = aAttr.value;
      }, this);
    }
    
    if (aTab.__SS_extdata)
      tabData.extData = aTab.__SS_extdata;
    else if (tabData.extData)
      delete tabData.extData;
    
    if (history && browser.docShell instanceof Ci.nsIDocShell)
      this._serializeSessionStorage(tabData, history, browser.docShell, aFullData);
    
    return tabData;
  },

  /**
   * Get an object that is a serialized representation of a History entry
   * Used for data storage
   * @param aEntry
   *        nsISHEntry instance
   * @param aFullData
   *        always return privacy sensitive data (use with care)
   * @returns object
   */
  _serializeHistoryEntry: function sss_serializeHistoryEntry(aEntry, aFullData) {
    var entry = { url: aEntry.URI.spec };
    
    if (aEntry.title && aEntry.title != entry.url) {
      entry.title = aEntry.title;
    }
    if (aEntry.isSubFrame) {
      entry.subframe = true;
    }
    if (!(aEntry instanceof Ci.nsISHEntry)) {
      return entry;
    }
    
    var cacheKey = aEntry.cacheKey;
    if (cacheKey && cacheKey instanceof Ci.nsISupportsPRUint32 &&
        cacheKey.data != 0) {
      // XXXbz would be better to have cache keys implement
      // nsISerializable or something.
      entry.cacheKey = cacheKey.data;
    }
    entry.ID = aEntry.ID;
    
    if (aEntry.contentType)
      entry.contentType = aEntry.contentType;
    
    var x = {}, y = {};
    aEntry.getScrollPosition(x, y);
    if (x.value != 0 || y.value != 0)
      entry.scroll = x.value + "," + y.value;
    
    try {
      var prefPostdata = this._prefBranch.getIntPref("sessionstore.postdata");
      if (aEntry.postData && (aFullData ||
            prefPostdata && this._checkPrivacyLevel(aEntry.URI.schemeIs("https")))) {
        aEntry.postData.QueryInterface(Ci.nsISeekableStream).
                        seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);
        var stream = Cc["@mozilla.org/binaryinputstream;1"].
                     createInstance(Ci.nsIBinaryInputStream);
        stream.setInputStream(aEntry.postData);
        var postBytes = stream.readByteArray(stream.available());
        var postdata = String.fromCharCode.apply(null, postBytes);
        if (aFullData || prefPostdata == -1 ||
            postdata.replace(/^(Content-.*\r\n)+(\r\n)*/, "").length <=
              prefPostdata) {
          // We can stop doing base64 encoding once our serialization into JSON
          // is guaranteed to handle all chars in strings, including embedded
          // nulls.
          entry.postdata_b64 = btoa(postdata);
        }
      }
    }
    catch (ex) { debug(ex); } // POSTDATA is tricky - especially since some extensions don't get it right

    if (aEntry.owner) {
      // Not catching anything specific here, just possible errors
      // from writeCompoundObject and the like.
      try {
        var binaryStream = Cc["@mozilla.org/binaryoutputstream;1"].
                           createInstance(Ci.nsIObjectOutputStream);
        var pipe = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);
        pipe.init(false, false, 0, 0xffffffff, null);
        binaryStream.setOutputStream(pipe.outputStream);
        binaryStream.writeCompoundObject(aEntry.owner, Ci.nsISupports, true);
        binaryStream.close();

        // Now we want to read the data from the pipe's input end and encode it.
        var scriptableStream = Cc["@mozilla.org/binaryinputstream;1"].
                               createInstance(Ci.nsIBinaryInputStream);
        scriptableStream.setInputStream(pipe.inputStream);
        var ownerBytes =
          scriptableStream.readByteArray(scriptableStream.available());
        // We can stop doing base64 encoding once our serialization into JSON
        // is guaranteed to handle all chars in strings, including embedded
        // nulls.
        entry.owner_b64 = btoa(String.fromCharCode.apply(null, ownerBytes));
      }
      catch (ex) { debug(ex); }
    }
    
    if (!(aEntry instanceof Ci.nsISHContainer)) {
      return entry;
    }
    
    if (aEntry.childCount > 0) {
      entry.children = [];
      for (var i = 0; i < aEntry.childCount; i++) {
        var child = aEntry.GetChildAt(i);
        if (child) {
          entry.children.push(this._serializeHistoryEntry(child, aFullData));
        }
        else { // to maintain the correct frame order, insert a dummy entry 
          entry.children.push({ url: "about:blank" });
        }
        // don't try to restore framesets containing wyciwyg URLs (cf. bug 424689 and bug 450595)
        if (/^wyciwyg:\/\//.test(entry.children[i].url)) {
          delete entry.children;
          break;
        }
      }
    }
    
    return entry;
  },

  /**
   * Updates all sessionStorage "super cookies"
   * @param aTabData
   *        The data object for a specific tab
   * @param aHistory
   *        That tab's session history
   * @param aDocShell
   *        That tab's docshell (containing the sessionStorage)
   * @param aFullData
   *        always return privacy sensitive data (use with care)
   */
  _serializeSessionStorage:
    function sss_serializeSessionStorage(aTabData, aHistory, aDocShell, aFullData) {
    let storageData = {};
    let hasContent = false;

    for (let i = 0; i < aHistory.count; i++) {
      let uri = aHistory.getEntryAtIndex(i, false).URI;
      // sessionStorage is saved per domain (cf. nsDocShell::GetSessionStorageForURI)
      let domain = uri.spec;
      try {
        if (uri.host)
          domain = uri.prePath;
      }
      catch (ex) { /* this throws for host-less URIs (such as about: or jar:) */ }
      if (storageData[domain] || !(aFullData || this._checkPrivacyLevel(uri.schemeIs("https"))))
        continue;

      let storage, storageItemCount = 0;
      try {
        storage = aDocShell.getSessionStorageForURI(uri);
        storageItemCount = storage.length;
      }
      catch (ex) { /* sessionStorage might throw if it's turned off, see bug 458954 */ }
      if (storageItemCount == 0)
        continue;

      let data = storageData[domain] = {};
      for (let j = 0; j < storageItemCount; j++) {
        try {
          let key = storage.key(j);
          let item = storage.getItem(key);
          data[key] = { value: item.value };
          if (uri.schemeIs("https") && item.secure)
            data[key].secure = true;
        }
        catch (ex) { /* XXXzeniko this currently throws for secured items (cf. bug 442048) */ }
      }
      hasContent = true;
    }

    if (hasContent)
      aTabData.storage = storageData;
  },

  /**
   * go through all tabs and store the current scroll positions
   * and innerHTML content of WYSIWYG editors
   * @param aWindow
   *        Window reference
   */
  _updateTextAndScrollData: function sss_updateTextAndScrollData(aWindow) {
    var browsers = aWindow.getBrowser().browsers;
    for (var i = 0; i < browsers.length; i++) {
      try {
        var tabData = this._windows[aWindow.__SSi].tabs[i];
        if (browsers[i].parentNode.__SS_data &&
            browsers[i].parentNode.__SS_data._tabStillLoading)
          continue; // ignore incompletely initialized tabs
        this._updateTextAndScrollDataForTab(aWindow, browsers[i], tabData);
      }
      catch (ex) { debug(ex); } // get as much data as possible, ignore failures (might succeed the next time)
    }
  },

  /**
   * go through all frames and store the current scroll positions
   * and innerHTML content of WYSIWYG editors
   * @param aWindow
   *        Window reference
   * @param aBrowser
   *        single browser reference
   * @param aTabData
   *        tabData object to add the information to
   * @param aFullData
   *        always return privacy sensitive data (use with care)
   */
  _updateTextAndScrollDataForTab:
    function sss_updateTextAndScrollDataForTab(aWindow, aBrowser, aTabData, aFullData) {
    var tabIndex = (aTabData.index || aTabData.entries.length) - 1;
    // entry data needn't exist for tabs just initialized with an incomplete session state
    if (!aTabData.entries[tabIndex])
      return;
    
    let selectedPageStyle = aBrowser.markupDocumentViewer.authorStyleDisabled ? "_nostyle" :
                            this._getSelectedPageStyle(aBrowser.contentWindow);
    if (selectedPageStyle)
      aTabData.pageStyle = selectedPageStyle;
    else if (aTabData.pageStyle)
      delete aTabData.pageStyle;
    
    this._updateTextAndScrollDataForFrame(aWindow, aBrowser.contentWindow,
                                          aTabData.entries[tabIndex],
                                          !aTabData._formDataSaved, aFullData);
    aTabData._formDataSaved = true;
    if (aBrowser.currentURI.spec == "about:config")
      aTabData.entries[tabIndex].formdata = {
        "#textbox": aBrowser.contentDocument.getElementById("textbox").wrappedJSObject.value
      };
  },

  /**
   * go through all subframes and store all form data, the current
   * scroll positions and innerHTML content of WYSIWYG editors
   * @param aWindow
   *        Window reference
   * @param aContent
   *        frame reference
   * @param aData
   *        part of a tabData object to add the information to
   * @param aUpdateFormData
   *        update all form data for this tab
   * @param aFullData
   *        always return privacy sensitive data (use with care)
   */
  _updateTextAndScrollDataForFrame:
    function sss_updateTextAndScrollDataForFrame(aWindow, aContent, aData,
                                                 aUpdateFormData, aFullData) {
    for (var i = 0; i < aContent.frames.length; i++) {
      if (aData.children && aData.children[i])
        this._updateTextAndScrollDataForFrame(aWindow, aContent.frames[i],
                                              aData.children[i], aUpdateFormData, aFullData);
    }
    var isHTTPS = this._getURIFromString((aContent.parent || aContent).
                                         document.location.href).schemeIs("https");
    if (aFullData || this._checkPrivacyLevel(isHTTPS) ||
        aContent.top.document.location.href == "about:sessionrestore") {
      if (aFullData || aUpdateFormData) {
        let formData = this._collectFormDataForFrame(aContent.document);
        if (formData)
          aData.formdata = formData;
        else if (aData.formdata)
          delete aData.formdata;
      }
      
      // designMode is undefined e.g. for XUL documents (as about:config)
      if ((aContent.document.designMode || "") == "on") {
        if (aData.innerHTML === undefined && !aFullData) {
          // we get no "input" events from iframes - listen for keypress here
          let _this = this;
          aContent.addEventListener("keypress", function(aEvent) {
            _this.saveStateDelayed(aWindow, 3000);
          }, true);
        }
        aData.innerHTML = aContent.document.body.innerHTML;
      }
    }
    aData.scroll = aContent.scrollX + "," + aContent.scrollY;
  },

  /**
   * determine the title of the currently enabled style sheet (if any)
   * and recurse through the frameset if necessary
   * @param   aContent is a frame reference
   * @returns the title style sheet determined to be enabled (empty string if none)
   */
  _getSelectedPageStyle: function sss_getSelectedPageStyle(aContent) {
    const forScreen = /(?:^|,)\s*(?:all|screen)\s*(?:,|$)/i;
    for (let i = 0; i < aContent.document.styleSheets.length; i++) {
      let ss = aContent.document.styleSheets[i];
      let media = ss.media.mediaText;
      if (!ss.disabled && ss.title && (!media || forScreen.test(media)))
        return ss.title
    }
    for (let i = 0; i < aContent.frames.length; i++) {
      let selectedPageStyle = this._getSelectedPageStyle(aContent.frames[i]);
      if (selectedPageStyle)
        return selectedPageStyle;
    }
    return "";
  },

  /**
   * collect the state of all form elements
   * @param aDocument
   *        document reference
   */
  _collectFormDataForFrame: function sss_collectFormDataForFrame(aDocument) {
    let formNodes = aDocument.evaluate(XPathHelper.restorableFormNodes, aDocument,
                                       XPathHelper.resolveNS,
                                       Ci.nsIDOMXPathResult.UNORDERED_NODE_ITERATOR_TYPE, null);
    let node = formNodes.iterateNext();
    if (!node)
      return null;
    
    let data = {};
    do {
      let id = node.id ? "#" + node.id : XPathHelper.generate(node);
      if (node instanceof Ci.nsIDOMHTMLInputElement)
        data[id] = node.type == "checkbox" || node.type == "radio" ? node.checked : node.value;
      else if (node instanceof Ci.nsIDOMHTMLTextAreaElement)
        data[id] = node.value;
      else if (!node.multiple)
        data[id] = node.selectedIndex;
      else {
        let options = Array.map(node.options, function(aOpt, aIx) aOpt.selected ? aIx : -1);
        data[id] = options.filter(function(aIx) aIx >= 0);
      }
    } while ((node = formNodes.iterateNext()));
    
    return data;
  },

  /**
   * store all hosts for a URL
   * @param aWindow
   *        Window reference
   */
  _updateCookieHosts: function sss_updateCookieHosts(aWindow) {
    var hosts = this._windows[aWindow.__SSi]._hosts = {};
    
    // get all possible subdomain levels for a given URL
    var _this = this;
    function extractHosts(aEntry) {
      if (/^https?:\/\/(?:[^@\/\s]+@)?([\w.-]+)/.test(aEntry.url) &&
        !hosts[RegExp.$1] && _this._checkPrivacyLevel(_this._getURIFromString(aEntry.url).schemeIs("https"))) {
        var host = RegExp.$1;
        var ix;
        for (ix = host.indexOf(".") + 1; ix; ix = host.indexOf(".", ix) + 1) {
          hosts[host.substr(ix)] = true;
        }
        hosts[host] = true;
      }
      else if (/^file:\/\/([^\/]*)/.test(aEntry.url)) {
        hosts[RegExp.$1] = true;
      }
      if (aEntry.children) {
        aEntry.children.forEach(extractHosts);
      }
    }
    
    this._windows[aWindow.__SSi].tabs.forEach(function(aTabData) { aTabData.entries.forEach(extractHosts); });
  },

  /**
   * Serialize cookie data
   * @param aWindows
   *        array of Window references
   */
  _updateCookies: function sss_updateCookies(aWindows) {
    var cookiesEnum = Cc["@mozilla.org/cookiemanager;1"].
                      getService(Ci.nsICookieManager).enumerator;
    // collect the cookies per window
    for (var i = 0; i < aWindows.length; i++)
      aWindows[i].cookies = [];
    
    // MAX_EXPIRY should be 2^63-1, but JavaScript can't handle that precision
    var MAX_EXPIRY = Math.pow(2, 62);
    while (cookiesEnum.hasMoreElements()) {
      var cookie = cookiesEnum.getNext().QueryInterface(Ci.nsICookie2);
      if (cookie.isSession && this._checkPrivacyLevel(cookie.isSecure)) {
        var jscookie = null;
        aWindows.forEach(function(aWindow) {
          if (aWindow._hosts && aWindow._hosts[cookie.rawHost]) {
            // serialize the cookie when it's first needed
            if (!jscookie) {
              jscookie = { host: cookie.host, value: cookie.value };
              // only add attributes with non-default values (saving a few bits)
              if (cookie.path) jscookie.path = cookie.path;
              if (cookie.name) jscookie.name = cookie.name;
              if (cookie.isSecure) jscookie.secure = true;
              if (cookie.isHttpOnly) jscookie.httponly = true;
              if (cookie.expiry < MAX_EXPIRY) jscookie.expiry = cookie.expiry;
            }
            aWindow.cookies.push(jscookie);
          }
        });
      }
    }
    
    // don't include empty cookie sections
    for (i = 0; i < aWindows.length; i++)
      if (aWindows[i].cookies.length == 0)
        delete aWindows[i].cookies;
  },

  /**
   * Store window dimensions, visibility, sidebar
   * @param aWindow
   *        Window reference
   */
  _updateWindowFeatures: function sss_updateWindowFeatures(aWindow) {
    var winData = this._windows[aWindow.__SSi];
    
    WINDOW_ATTRIBUTES.forEach(function(aAttr) {
      winData[aAttr] = this._getWindowDimension(aWindow, aAttr);
    }, this);
    
    var hidden = WINDOW_HIDEABLE_FEATURES.filter(function(aItem) {
      return aWindow[aItem] && !aWindow[aItem].visible;
    });
    if (hidden.length != 0)
      winData.hidden = hidden.join(",");
    else if (winData.hidden)
      delete winData.hidden;

    var sidebar = aWindow.document.getElementById("sidebar-box").getAttribute("sidebarcommand");
    if (sidebar)
      winData.sidebar = sidebar;
    else if (winData.sidebar)
      delete winData.sidebar;
  },

  /**
   * serialize session data as Ini-formatted string
   * @param aUpdateAll
   *        Bool update all windows 
   * @returns string
   */
  _getCurrentState: function sss_getCurrentState(aUpdateAll) {
    var activeWindow = this._getMostRecentBrowserWindow();
    
    if (this._loadState == STATE_RUNNING) {
      // update the data for all windows with activities since the last save operation
      this._forEachBrowserWindow(function(aWindow) {
        if (!this._isWindowLoaded(aWindow)) // window data is still in _statesToRestore
          return;
        if (aUpdateAll || this._dirtyWindows[aWindow.__SSi] || aWindow == activeWindow) {
          this._collectWindowData(aWindow);
        }
        else { // always update the window features (whose change alone never triggers a save operation)
          this._updateWindowFeatures(aWindow);
        }
      }, this);
      this._dirtyWindows = [];
    }
    
    // collect the data for all windows
    var total = [], windows = [];
    var nonPopupCount = 0;
    var ix;
    for (ix in this._windows) {
      total.push(this._windows[ix]);
      windows.push(ix);
      if (!this._windows[ix].isPopup)
        nonPopupCount++;
    }
    this._updateCookies(total);
    
    // collect the data for all windows yet to be restored
    for (ix in this._statesToRestore) {
      for each (let winData in this._statesToRestore[ix].windows) {
        total.push(winData);
        if (!winData.isPopup)
          nonPopupCount++;
      }
    }
    
    // if no non-popup browser window remains open, return the state of the last closed window(s)
    if (nonPopupCount == 0 && this._lastClosedWindows) {
      // prepend the last non-popup browser window, so that if the user loads more tabs
      // at startup we don't accidentally add them to a popup window
      total = this._lastClosedWindows.concat(total);
    }
    if (activeWindow) {
      this.activeWindowSSiCache = activeWindow.__SSi || "";
    }
    ix = this.activeWindowSSiCache ? windows.indexOf(this.activeWindowSSiCache) : -1;

    return { windows: total, selectedWindow: ix + 1 };
  },

  /**
   * serialize session data for a window 
   * @param aWindow
   *        Window reference
   * @returns string
   */
  _getWindowState: function sss_getWindowState(aWindow) {
    if (!this._isWindowLoaded(aWindow))
      return this._statesToRestore[aWindow.__SS_restoreID];
    
    if (this._loadState == STATE_RUNNING) {
      this._collectWindowData(aWindow);
    }
    
    var total = [this._windows[aWindow.__SSi]];
    this._updateCookies(total);
    
    return { windows: total };
  },

  _collectWindowData: function sss_collectWindowData(aWindow) {
    if (!this._isWindowLoaded(aWindow))
      return;
    
    // update the internal state data for this window
    this._saveWindowHistory(aWindow);
    this._updateTextAndScrollData(aWindow);
    this._updateCookieHosts(aWindow);
    this._updateWindowFeatures(aWindow);
    
    this._dirtyWindows[aWindow.__SSi] = false;
  },

/* ........ Restoring Functionality .............. */

  /**
   * restore features to a single window
   * @param aWindow
   *        Window reference
   * @param aState
   *        JS object or its eval'able source
   * @param aOverwriteTabs
   *        bool overwrite existing tabs w/ new ones
   * @param aFollowUp
   *        bool this isn't the restoration of the first window
   */
  restoreWindow: function sss_restoreWindow(aWindow, aState, aOverwriteTabs, aFollowUp) {
    if (!aFollowUp) {
      this.windowToFocus = aWindow;
    }
    // initialize window if necessary
    if (aWindow && (!aWindow.__SSi || !this._windows[aWindow.__SSi]))
      this.onLoad(aWindow);

    try {
      var root = typeof aState == "string" ? this._safeEval(aState) : aState;
      if (!root.windows[0]) {
        this._notifyIfAllWindowsRestored();
        return; // nothing to restore
      }
    }
    catch (ex) { // invalid state object - don't restore anything 
      debug(ex);
      this._notifyIfAllWindowsRestored();
      return;
    }
    
    var winData;
    if (!aState.selectedWindow) {
      aState.selectedWindow = 0;
    }
    // open new windows for all further window entries of a multi-window session
    // (unless they don't contain any tab data)
    for (var w = 1; w < root.windows.length; w++) {
      winData = root.windows[w];
      if (winData && winData.tabs && winData.tabs[0]) {
        var window = this._openWindowWithState({ windows: [winData] });
        if (w == aState.selectedWindow - 1) {
          this.windowToFocus = window;
        }
      }
    }
    winData = root.windows[0];
    if (!winData.tabs) {
      winData.tabs = [];
    }
    // don't restore a single blank tab when we've had an external
    // URL passed in for loading at startup (cf. bug 357419)
    else if (root._firstTabs && !aOverwriteTabs && winData.tabs.length == 1 &&
             (!winData.tabs[0].entries || winData.tabs[0].entries.length == 0)) {
      winData.tabs = [];
    }
    
    var tabbrowser = aWindow.getBrowser();
    var openTabCount = aOverwriteTabs ? tabbrowser.browsers.length : -1;
    var newTabCount = winData.tabs.length;
    let tabs = [];
    
    for (var t = 0; t < newTabCount; t++) {
      tabs.push(t < openTabCount ? tabbrowser.mTabs[t] : tabbrowser.addTab());
      // when resuming at startup: add additionally requested pages to the end
      if (!aOverwriteTabs && root._firstTabs) {
        tabbrowser.moveTabTo(tabs[t], t);
      }
    }

    // when overwriting tabs, remove all superflous ones
    for (t = openTabCount - 1; t >= newTabCount; t--) {
      tabbrowser.removeTab(tabbrowser.mTabs[t]);
    }
    
    if (aOverwriteTabs) {
      this.restoreWindowFeatures(aWindow, winData);
    }
    if (winData.cookies) {
      this.restoreCookies(winData.cookies);
    }
    if (winData.extData) {
      if (!this._windows[aWindow.__SSi].extData) {
        this._windows[aWindow.__SSi].extData = {}
      }
      for (var key in winData.extData) {
        this._windows[aWindow.__SSi].extData[key] = winData.extData[key];
      }
    }
    if (winData._closedTabs && (root._firstTabs || aOverwriteTabs)) {
      this._windows[aWindow.__SSi]._closedTabs = winData._closedTabs;
    }
    
    this.restoreHistoryPrecursor(aWindow, tabs, winData.tabs,
      (aOverwriteTabs ? (parseInt(winData.selected) || 1) : 0), 0, 0);

    this._notifyIfAllWindowsRestored();
  },

  /**
   * Manage history restoration for a window
   * @param aWindow
   *        Window to restore the tabs into
   * @param aTabs
   *        Array of tab references
   * @param aTabData
   *        Array of tab data
   * @param aSelectTab
   *        Index of selected tab
   * @param aIx
   *        Index of the next tab to check readyness for
   * @param aCount
   *        Counter for number of times delaying b/c browser or history aren't ready
   */
  restoreHistoryPrecursor:
    function sss_restoreHistoryPrecursor(aWindow, aTabs, aTabData, aSelectTab, aIx, aCount) {
    var tabbrowser = aWindow.getBrowser();
    
    // make sure that all browsers and their histories are available
    // - if one's not, resume this check in 100ms (repeat at most 10 times)
    for (var t = aIx; t < aTabs.length; t++) {
      try {
        if (!tabbrowser.getBrowserForTab(aTabs[t]).webNavigation.sessionHistory) {
          throw new Error();
        }
      }
      catch (ex) { // in case browser or history aren't ready yet 
        if (aCount < 10) {
          var restoreHistoryFunc = function(self) {
            self.restoreHistoryPrecursor(aWindow, aTabs, aTabData, aSelectTab, aIx, aCount + 1);
          }
          aWindow.setTimeout(restoreHistoryFunc, 100, this);
          return;
        }
      }
    }
    
    // mark the tabs as loading
    for (t = 0; t < aTabs.length; t++) {
      var tab = aTabs[t];
      var browser = tabbrowser.getBrowserForTab(tab);
      
      aTabData[t]._tabStillLoading = true;
      if (!aTabData[t].entries || aTabData[t].entries.length == 0) {
        // make sure to blank out this tab's content
        // (just purging the tab's history won't be enough)
        browser.contentDocument.location = "about:blank";
        continue;
      }
      
      browser.stop(); // in case about:blank isn't done yet
      
      tab.setAttribute("busy", "true");
      tabbrowser.updateIcon(tab);
      tabbrowser.setTabTitleLoading(tab);
      
      // wall-paper fix for bug 439675: make sure that the URL to be loaded
      // is always visible in the address bar
      let activeIndex = (aTabData[t].index || aTabData[t].entries.length) - 1;
      let activePageData = aTabData[t].entries[activeIndex] || null;
      browser.userTypedValue = activePageData ? activePageData.url || null : null;
      
      // keep the data around to prevent dataloss in case
      // a tab gets closed before it's been properly restored
      browser.parentNode.__SS_data = aTabData[t];
    }
    
    // make sure to restore the selected tab first (if any)
    if (aSelectTab-- && aTabs[aSelectTab]) {
        aTabs.unshift(aTabs.splice(aSelectTab, 1)[0]);
        aTabData.unshift(aTabData.splice(aSelectTab, 1)[0]);
        tabbrowser.selectedTab = aTabs[0];
    }

    if (!this._isWindowLoaded(aWindow)) {
      // from now on, the data will come from the actual window
      delete this._statesToRestore[aWindow.__SS_restoreID];
      delete aWindow.__SS_restoreID;
    }
    
    // helper hash for ensuring unique frame IDs
    var idMap = { used: {} };
    this.restoreHistory(aWindow, aTabs, aTabData, idMap);
  },

  /**
   * Restory history for a window
   * @param aWindow
   *        Window reference
   * @param aTabs
   *        Array of tab references
   * @param aTabData
   *        Array of tab data
   * @param aIdMap
   *        Hash for ensuring unique frame IDs
   */
  restoreHistory: function sss_restoreHistory(aWindow, aTabs, aTabData, aIdMap) {
    var _this = this;
    while (aTabs.length > 0 && (!aTabData[0]._tabStillLoading || !aTabs[0].parentNode)) {
      aTabs.shift(); // this tab got removed before being completely restored
      aTabData.shift();
    }
    if (aTabs.length == 0) {
      return; // no more tabs to restore
    }
    
    var tab = aTabs.shift();
    var tabData = aTabData.shift();

    var browser = aWindow.getBrowser().getBrowserForTab(tab);
    var history = browser.webNavigation.sessionHistory;
    
    if (history.count > 0) {
      history.PurgeHistory(history.count);
    }
    history.QueryInterface(Ci.nsISHistoryInternal);
    
    if (!tabData.entries) {
      tabData.entries = [];
    }
    if (tabData.extData) {
      tab.__SS_extdata = {};
      for (let key in tabData.extData)
        tab.__SS_extdata[key] = tabData.extData[key];
    }
    else
      delete tab.__SS_extdata;
    
    for (var i = 0; i < tabData.entries.length; i++) {
      history.addEntry(this._deserializeHistoryEntry(tabData.entries[i], aIdMap), true);
    }
    
    // make sure to reset the capabilities and attributes, in case this tab gets reused
    var disallow = (tabData.disallow)?tabData.disallow.split(","):[];
    CAPABILITIES.forEach(function(aCapability) {
      browser.docShell["allow" + aCapability] = disallow.indexOf(aCapability) == -1;
    });
    Array.filter(tab.attributes, function(aAttr) {
      return (_this.xulAttributes.indexOf(aAttr.name) > -1);
    }).forEach(tab.removeAttribute, tab);
    if (tabData.xultab) {
      // restore attributes from the legacy Firefox 2.0/3.0 format
      tabData.xultab.split(" ").forEach(function(aAttr) {
        if (/^([^\s=]+)=(.*)/.test(aAttr)) {
          tab.setAttribute(RegExp.$1, decodeURI(RegExp.$2));
        }
      });
    }
    for (let name in tabData.attributes)
      tab.setAttribute(name, tabData.attributes[name]);
    
    if (tabData.storage && browser.docShell instanceof Ci.nsIDocShell)
      this._deserializeSessionStorage(tabData.storage, browser.docShell);
    
    // notify the tabbrowser that the tab chrome has been restored
    var event = aWindow.document.createEvent("Events");
    event.initEvent("SSTabRestoring", true, false);
    tab.dispatchEvent(event);
    
    let activeIndex = (tabData.index || tabData.entries.length) - 1;
    if (activeIndex >= tabData.entries.length)
      activeIndex = tabData.entries.length - 1;
    try {
      if (activeIndex >= 0)
        browser.webNavigation.gotoIndex(activeIndex);
    }
    catch (ex) {
      // ignore page load errors
      tab.removeAttribute("busy");
    }
    
    if (tabData.entries.length > 0) {
      // restore those aspects of the currently active documents
      // which are not preserved in the plain history entries
      // (mainly scroll state and text data)
      browser.__SS_restore_data = tabData.entries[activeIndex] || {};
      browser.__SS_restore_text = tabData.text || "";
      browser.__SS_restore_pageStyle = tabData.pageStyle || "";
      browser.__SS_restore_tab = tab;
      browser.__SS_restore = this.restoreDocument_proxy;
      browser.addEventListener("load", browser.__SS_restore, true);
    }
    
    aWindow.setTimeout(function(){ _this.restoreHistory(aWindow, aTabs, aTabData, aIdMap); }, 0);
  },

  /**
   * expands serialized history data into a session-history-entry instance
   * @param aEntry
   *        Object containing serialized history data for a URL
   * @param aIdMap
   *        Hash for ensuring unique frame IDs
   * @returns nsISHEntry
   */
  _deserializeHistoryEntry: function sss_deserializeHistoryEntry(aEntry, aIdMap) {
    var shEntry = Cc["@mozilla.org/browser/session-history-entry;1"].
                  createInstance(Ci.nsISHEntry);
    
    var ioService = Cc["@mozilla.org/network/io-service;1"].
                    getService(Ci.nsIIOService);
    shEntry.setURI(ioService.newURI(aEntry.url, null, null));
    shEntry.setTitle(aEntry.title || aEntry.url);
    if (aEntry.subframe)
      shEntry.setIsSubFrame(aEntry.subframe || false);
    shEntry.loadType = Ci.nsIDocShellLoadInfo.loadHistory;
    if (aEntry.contentType)
      shEntry.contentType = aEntry.contentType;
    
    if (aEntry.cacheKey) {
      var cacheKey = Cc["@mozilla.org/supports-PRUint32;1"].
                     createInstance(Ci.nsISupportsPRUint32);
      cacheKey.data = aEntry.cacheKey;
      shEntry.cacheKey = cacheKey;
    }

    if (aEntry.ID) {
      // get a new unique ID for this frame (since the one from the last
      // start might already be in use)
      var id = aIdMap[aEntry.ID] || 0;
      if (!id) {
        for (id = Date.now(); aIdMap.used[id]; id++);
        aIdMap[aEntry.ID] = id;
        aIdMap.used[id] = true;
      }
      shEntry.ID = id;
    }
    
    if (aEntry.scroll) {
      var scrollPos = (aEntry.scroll || "0,0").split(",");
      scrollPos = [parseInt(scrollPos[0]) || 0, parseInt(scrollPos[1]) || 0];
      shEntry.setScrollPosition(scrollPos[0], scrollPos[1]);
    }

    var postdata;
    if (aEntry.postdata_b64) {  // Firefox 3
      postdata = atob(aEntry.postdata_b64);
    } else if (aEntry.postdata) { // Firefox 2
      postdata = aEntry.postdata;
    }

    if (postdata) {
      var stream = Cc["@mozilla.org/io/string-input-stream;1"].
                   createInstance(Ci.nsIStringInputStream);
      stream.setData(postdata, postdata.length);
      shEntry.postData = stream;
    }

    if (aEntry.owner_b64) {  // Firefox 3
      var ownerInput = Cc["@mozilla.org/io/string-input-stream;1"].
                       createInstance(Ci.nsIStringInputStream);
      var binaryData = atob(aEntry.owner_b64);
      ownerInput.setData(binaryData, binaryData.length);
      var binaryStream = Cc["@mozilla.org/binaryinputstream;1"].
                         createInstance(Ci.nsIObjectInputStream);
      binaryStream.setInputStream(ownerInput);
      try { // Catch possible deserialization exceptions
        shEntry.owner = binaryStream.readObject(true);
      } catch (ex) { debug(ex); }
    } else if (aEntry.ownerURI) { // Firefox 2
      var uriObj = ioService.newURI(aEntry.ownerURI, null, null);
      shEntry.owner = Cc["@mozilla.org/scriptsecuritymanager;1"].
                      getService(Ci.nsIScriptSecurityManager).
                      getCodebasePrincipal(uriObj);
    }
    
    if (aEntry.children && shEntry instanceof Ci.nsISHContainer) {
      for (var i = 0; i < aEntry.children.length; i++) {
        shEntry.AddChild(this._deserializeHistoryEntry(aEntry.children[i], aIdMap), i);
      }
    }
    
    return shEntry;
  },

  /**
   * restores all sessionStorage "super cookies"
   * @param aStorageData
   *        Storage data to be restored
   * @param aDocShell
   *        A tab's docshell (containing the sessionStorage)
   */
  _deserializeSessionStorage: function sss_deserializeSessionStorage(aStorageData, aDocShell) {
    let ioService = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
    for (let url in aStorageData) {
      let uri = ioService.newURI(url, null, null);
      let storage = aDocShell.getSessionStorageForURI(uri);
      for (let key in aStorageData[url]) {
        try {
          storage.setItem(key, aStorageData[url][key].value);
          if (uri.schemeIs("https"))
            storage.getItem(key).secure = aStorageData[url][key].secure || false;
        }
        catch (ex) { Cu.reportError(ex); } // throws e.g. for URIs that can't have sessionStorage
      }
    }
  },

  /**
   * Restore properties to a loaded document
   */
  restoreDocument_proxy: function sss_restoreDocument_proxy(aEvent) {
    // wait for the top frame to be loaded completely
    if (!aEvent || !aEvent.originalTarget || !aEvent.originalTarget.defaultView || aEvent.originalTarget.defaultView != aEvent.originalTarget.defaultView.top) {
      return;
    }
    
    // always call this before injecting content into a document!
    function hasExpectedURL(aDocument, aURL)
      !aURL || aURL.replace(/#.*/, "") == aDocument.location.href.replace(/#.*/, "");
    
    // restore text data saved by Firefox 2.0/3.0
    var textArray = this.__SS_restore_text ? this.__SS_restore_text.split(" ") : [];
    function restoreTextData(aContent, aPrefix, aURL) {
      textArray.forEach(function(aEntry) {
        if (/^((?:\d+\|)*)(#?)([^\s=]+)=(.*)$/.test(aEntry) &&
            RegExp.$1 == aPrefix && hasExpectedURL(aContent.document, aURL)) {
          var document = aContent.document;
          var node = RegExp.$2 ? document.getElementById(RegExp.$3) : document.getElementsByName(RegExp.$3)[0] || null;
          if (node && "value" in node) {
            node.value = decodeURI(RegExp.$4);
            
            var event = document.createEvent("UIEvents");
            event.initUIEvent("input", true, true, aContent, 0);
            node.dispatchEvent(event);
          }
        }
      });
    }
    
    function restoreFormData(aDocument, aData, aURL) {
      for (let key in aData) {
        if (!hasExpectedURL(aDocument, aURL))
          return;
        
        let node = key.charAt(0) == "#" ? aDocument.getElementById(key.slice(1)) :
                                          XPathHelper.resolve(aDocument, key);
        if (!node)
          continue;
        
        let value = aData[key];
        if (typeof value == "string") {
          node.value = value;
          
          let event = aDocument.createEvent("UIEvents");
          event.initUIEvent("input", true, true, aDocument.defaultView, 0);
          node.dispatchEvent(event);
        }
        else if (typeof value == "boolean")
          node.checked = value;
        else if (typeof value == "number")
          try {
            node.selectedIndex = value;
          } catch (ex) { /* throws for invalid indices */ }
        else if (value && typeof value.indexOf == "function" && node.options) {
          Array.forEach(node.options, function(aOpt, aIx) {
            aOpt.selected = value.indexOf(aIx) > -1;
          });
        }
        // NB: dispatching "change" events might have unintended side-effects
      }
    }
    
    let selectedPageStyle = this.__SS_restore_pageStyle;
    let window = this.ownerDocument.defaultView;
    function restoreTextDataAndScrolling(aContent, aData, aPrefix) {
      if (aData.formdata)
        restoreFormData(aContent.document, aData.formdata, aData.url);
      else
        restoreTextData(aContent, aPrefix, aData.url);
      if (aData.innerHTML) {
        window.setTimeout(function() {
          if (aContent.document.designMode == "on" &&
              hasExpectedURL(aContent.document, aData.url)) {
            aContent.document.body.innerHTML = aData.innerHTML;
          }
        }, 0);
      }
      if (aData.scroll && /(\d+),(\d+)/.test(aData.scroll)) {
        aContent.scrollTo(RegExp.$1, RegExp.$2);
      }
      Array.forEach(aContent.document.styleSheets, function(aSS) {
        aSS.disabled = aSS.title && aSS.title != selectedPageStyle;
      });
      for (var i = 0; i < aContent.frames.length; i++) {
        if (aData.children && aData.children[i] &&
          hasExpectedURL(aContent.document, aData.url)) {
          restoreTextDataAndScrolling(aContent.frames[i], aData.children[i], aPrefix + i + "|");
        }
      }
    }
    
    // don't restore text data and scrolling state if the user has navigated
    // away before the loading completed (except for in-page navigation)
    if (hasExpectedURL(aEvent.originalTarget, this.__SS_restore_data.url)) {
      var content = aEvent.originalTarget.defaultView;
      if (this.currentURI.spec == "about:config") {
        // unwrap the document for about:config because otherwise the properties
        // of the XBL bindings - as the textbox - aren't accessible (see bug 350718)
        content = content.wrappedJSObject;
      }
      restoreTextDataAndScrolling(content, this.__SS_restore_data, "");
      this.markupDocumentViewer.authorStyleDisabled = selectedPageStyle == "_nostyle";
      
      // notify the tabbrowser that this document has been completely restored
      var event = this.ownerDocument.createEvent("Events");
      event.initEvent("SSTabRestored", true, false);
      this.__SS_restore_tab.dispatchEvent(event);
    }
    
    this.removeEventListener("load", this.__SS_restore, true);
    delete this.__SS_restore_data;
    delete this.__SS_restore_text;
    delete this.__SS_restore_pageStyle;
    delete this.__SS_restore_tab;
    delete this.__SS_restore;
  },

  /**
   * Restore visibility and dimension features to a window
   * @param aWindow
   *        Window reference
   * @param aWinData
   *        Object containing session data for the window
   */
  restoreWindowFeatures: function sss_restoreWindowFeatures(aWindow, aWinData) {
    var hidden = (aWinData.hidden)?aWinData.hidden.split(","):[];
    WINDOW_HIDEABLE_FEATURES.forEach(function(aItem) {
      aWindow[aItem].visible = hidden.indexOf(aItem) == -1;
    });
    
    if (aWinData.isPopup)
      this._windows[aWindow.__SSi].isPopup = true;
    else
      delete this._windows[aWindow.__SSi].isPopup;
    
    var _this = this;
    aWindow.setTimeout(function() {
      _this.restoreDimensions.apply(_this, [aWindow, aWinData.width || 0, 
        aWinData.height || 0, "screenX" in aWinData ? aWinData.screenX : NaN,
        "screenY" in aWinData ? aWinData.screenY : NaN,
        aWinData.sizemode || "", aWinData.sidebar || ""]);
    }, 0);
  },

  /**
   * Restore a window's dimensions
   * @param aWidth
   *        Window width
   * @param aHeight
   *        Window height
   * @param aLeft
   *        Window left
   * @param aTop
   *        Window top
   * @param aSizeMode
   *        Window size mode (eg: maximized)
   * @param aSidebar
   *        Sidebar command
   */
  restoreDimensions: function sss_restoreDimensions(aWindow, aWidth, aHeight, aLeft, aTop, aSizeMode, aSidebar) {
    var win = aWindow;
    var _this = this;
    function win_(aName) { return _this._getWindowDimension(win, aName); }
    
    // only modify those aspects which aren't correct yet
    if (aWidth && aHeight && (aWidth != win_("width") || aHeight != win_("height"))) {
      aWindow.resizeTo(aWidth, aHeight);
    }
    if (!isNaN(aLeft) && !isNaN(aTop) && (aLeft != win_("screenX") || aTop != win_("screenY"))) {
      aWindow.moveTo(aLeft, aTop);
    }
    if (aSizeMode == "maximized" && win_("sizemode") != "maximized") {
      aWindow.maximize();
    }
    else if (aSizeMode && aSizeMode != "maximized" && win_("sizemode") != "normal") {
      aWindow.restore();
    }
    var sidebar = aWindow.document.getElementById("sidebar-box");
    if (sidebar.getAttribute("sidebarcommand") != aSidebar) {
      aWindow.toggleSidebar(aSidebar);
    }
    // since resizing/moving a window brings it to the foreground,
    // we might want to re-focus the last focused window
    if (this.windowToFocus) {
      this.windowToFocus.content.focus();
    }
  },

  /**
   * Restores cookies (accepting both Firefox 2.0 and current format)
   * @param aCookies
   *        Array of cookie objects
   */
  restoreCookies: function sss_restoreCookies(aCookies) {
    if (aCookies.count && aCookies.domain1) {
      // convert to the new cookie serialization format
      var converted = [];
      for (var i = 1; i <= aCookies.count; i++) {
        // for simplicity we only accept the format we produced ourselves
        var parsed = aCookies["value" + i].match(/^([^=;]+)=([^;]*);(?:domain=[^;]+;)?(?:path=([^;]*);)?(secure;)?(httponly;)?/);
        if (parsed && /^https?:\/\/([^\/]+)/.test(aCookies["domain" + i]))
          converted.push({
            host: RegExp.$1, path: parsed[3], name: parsed[1], value: parsed[2],
            secure: parsed[4], httponly: parsed[5]
          });
      }
      aCookies = converted;
    }
    
    var cookieManager = Cc["@mozilla.org/cookiemanager;1"].
                        getService(Ci.nsICookieManager2);
    // MAX_EXPIRY should be 2^63-1, but JavaScript can't handle that precision
    var MAX_EXPIRY = Math.pow(2, 62);
    for (i = 0; i < aCookies.length; i++) {
      var cookie = aCookies[i];
      try {
        cookieManager.add(cookie.host, cookie.path || "", cookie.name || "", cookie.value, !!cookie.secure, !!cookie.httponly, true, "expiry" in cookie ? cookie.expiry : MAX_EXPIRY);
      }
      catch (ex) { Cu.reportError(ex); } // don't let a single cookie stop recovering
    }
  },

/* ........ Disk Access .............. */

  /**
   * save state delayed by N ms
   * marks window as dirty (i.e. data update can't be skipped)
   * @param aWindow
   *        Window reference
   * @param aDelay
   *        Milliseconds to delay
   */
  saveStateDelayed: function sss_saveStateDelayed(aWindow, aDelay) {
    if (aWindow) {
      this._dirtyWindows[aWindow.__SSi] = true;
    }

    if (!this._saveTimer && this._resume_from_crash &&
        !this._inPrivateBrowsing) {
      // interval until the next disk operation is allowed
      var minimalDelay = this._lastSaveTime + this._interval - Date.now();
      
      // if we have to wait, set a timer, otherwise saveState directly
      aDelay = Math.max(minimalDelay, aDelay || 2000);
      if (aDelay > 0) {
        this._saveTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        this._saveTimer.init(this, aDelay, Ci.nsITimer.TYPE_ONE_SHOT);
      }
      else {
        this.saveState();
      }
    }
  },

  /**
   * save state to disk
   * @param aUpdateAll
   *        Bool update all windows 
   */
  saveState: function sss_saveState(aUpdateAll) {
    // if crash recovery is disabled, only save session resuming information
    if (!this._resume_from_crash && this._loadState == STATE_RUNNING)
      return;

    // if we're in private browsing mode, do nothing
    if (this._inPrivateBrowsing)
      return;

    var oState = this._getCurrentState(aUpdateAll);
    oState.session = {
      state: this._loadState == STATE_RUNNING ? STATE_RUNNING_STR : STATE_STOPPED_STR,
      lastUpdate: Date.now()
    };
    if (this._recentCrashes)
      oState.session.recentCrashes = this._recentCrashes;

    this._saveStateObject(oState);
  },

  /**
   * write a state object to disk
   */
  _saveStateObject: function sss_saveStateObject(aStateObj) {
    var stateString = Cc["@mozilla.org/supports-string;1"].
                        createInstance(Ci.nsISupportsString);
    // parentheses are for backwards compatibility with Firefox 2.0 and 3.0
    stateString.data = "(" + this._toJSONString(aStateObj) + ")";

    var observerService = Cc["@mozilla.org/observer-service;1"].
                          getService(Ci.nsIObserverService);
    observerService.notifyObservers(stateString, "sessionstore-state-write", "");

    // don't touch the file if an observer has deleted all state data
    if (stateString.data)
      this._writeFile(this._sessionFile, stateString.data);

    this._lastSaveTime = Date.now();
  },

  /**
   * delete session datafile and backup
   */
  _clearDisk: function sss_clearDisk() {
    if (this._sessionFile.exists()) {
      try {
        this._sessionFile.remove(false);
      }
      catch (ex) { dump(ex + '\n'); } // couldn't remove the file - what now?
    }
    if (this._sessionFileBackup.exists()) {
      try {
        this._sessionFileBackup.remove(false);
      }
      catch (ex) { dump(ex + '\n'); } // couldn't remove the file - what now?
    }
  },

/* ........ Auxiliary Functions .............. */

  /**
   * call a callback for all currently opened browser windows
   * (might miss the most recent one)
   * @param aFunc
   *        Callback each window is passed to
   */
  _forEachBrowserWindow: function sss_forEachBrowserWindow(aFunc) {
    var windowMediator = Cc["@mozilla.org/appshell/window-mediator;1"].
                         getService(Ci.nsIWindowMediator);
    var windowsEnum = windowMediator.getEnumerator("navigator:browser");
    
    while (windowsEnum.hasMoreElements()) {
      var window = windowsEnum.getNext();
      if (window.__SSi) {
        aFunc.call(this, window);
      }
    }
  },

  /**
   * Returns most recent window
   * @returns Window reference
   */
  _getMostRecentBrowserWindow: function sss_getMostRecentBrowserWindow() {
    var windowMediator = Cc["@mozilla.org/appshell/window-mediator;1"].
                         getService(Ci.nsIWindowMediator);
    return windowMediator.getMostRecentWindow("navigator:browser");
  },

  /**
   * open a new browser window for a given session state
   * called when restoring a multi-window session
   * @param aState
   *        Object containing session data
   */
  _openWindowWithState: function sss_openWindowWithState(aState) {
    var argString = Cc["@mozilla.org/supports-string;1"].
                    createInstance(Ci.nsISupportsString);
    argString.data = "";

    //XXXzeniko shouldn't it be possible to set the window's dimensions here (as feature)?
    var window = Cc["@mozilla.org/embedcomp/window-watcher;1"].
                 getService(Ci.nsIWindowWatcher).
                 openWindow(null, this._prefBranch.getCharPref("chromeURL"), "_blank",
                            "chrome,dialog=no,all", argString);
    
    do {
      var ID = "window" + Math.random();
    } while (ID in this._statesToRestore);
    this._statesToRestore[(window.__SS_restoreID = ID)] = aState;
    
    return window;
  },

  /**
   * Whether or not to resume session, if not recovering from a crash.
   * @returns bool
   */
  _doResumeSession: function sss_doResumeSession() {
    return this._prefBranch.getIntPref("startup.page") == 3 ||
      this._prefBranch.getBoolPref("sessionstore.resume_session_once");
  },

  /**
   * whether the user wants to load any other page at startup
   * (except the homepage) - needed for determining whether to overwrite the current tabs
   * C.f.: nsBrowserContentHandler's defaultArgs implementation.
   * @returns bool
   */
  _isCmdLineEmpty: function sss_isCmdLineEmpty(aWindow) {
    var defaultArgs = Cc["@mozilla.org/browser/clh;1"].
                      getService(Ci.nsIBrowserHandler).defaultArgs;
    if (aWindow.arguments && aWindow.arguments[0] &&
        aWindow.arguments[0] == defaultArgs)
      aWindow.arguments[0] = null;

    return !aWindow.arguments || !aWindow.arguments[0];
  },

  /**
   * don't save sensitive data if the user doesn't want to
   * (distinguishes between encrypted and non-encrypted sites)
   * @param aIsHTTPS
   *        Bool is encrypted
   * @returns bool
   */
  _checkPrivacyLevel: function sss_checkPrivacyLevel(aIsHTTPS) {
    return this._prefBranch.getIntPref("sessionstore.privacy_level") < (aIsHTTPS ? PRIVACY_ENCRYPTED : PRIVACY_FULL);
  },

  /**
   * on popup windows, the XULWindow's attributes seem not to be set correctly
   * we use thus JSDOMWindow attributes for sizemode and normal window attributes
   * (and hope for reasonable values when maximized/minimized - since then
   * outerWidth/outerHeight aren't the dimensions of the restored window)
   * @param aWindow
   *        Window reference
   * @param aAttribute
   *        String sizemode | width | height | other window attribute
   * @returns string
   */
  _getWindowDimension: function sss_getWindowDimension(aWindow, aAttribute) {
    if (aAttribute == "sizemode") {
      switch (aWindow.windowState) {
      case aWindow.STATE_MAXIMIZED:
        return "maximized";
      case aWindow.STATE_MINIMIZED:
        return "minimized";
      default:
        return "normal";
      }
    }
    
    var dimension;
    switch (aAttribute) {
    case "width":
      dimension = aWindow.outerWidth;
      break;
    case "height":
      dimension = aWindow.outerHeight;
      break;
    default:
      dimension = aAttribute in aWindow ? aWindow[aAttribute] : "";
      break;
    }
    
    if (aWindow.windowState == aWindow.STATE_NORMAL) {
      return dimension;
    }
    return aWindow.document.documentElement.getAttribute(aAttribute) || dimension;
  },

  /**
   * Get nsIURI from string
   * @param string
   * @returns nsIURI
   */
  _getURIFromString: function sss_getURIFromString(aString) {
    var ioService = Cc["@mozilla.org/network/io-service;1"].
                    getService(Ci.nsIIOService);
    return ioService.newURI(aString, null, null);
  },

  /**
   * Annotate a breakpad crash report with the currently selected tab's URL.
   */
  _updateCrashReportURL: function sss_updateCrashReportURL(aWindow) {
    if (!Ci.nsICrashReporter) {
      // if breakpad isn't built, don't bother next time at all
      this._updateCrashReportURL = function(aWindow) {};
      return;
    }
    try {
      var currentURI = aWindow.getBrowser().currentURI.clone();
      // if the current URI contains a username/password, remove it
      try { 
        currentURI.userPass = ""; 
      } 
      catch (ex) { } // ignore failures on about: URIs

      var cr = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsICrashReporter);
      cr.annotateCrashReport("URL", currentURI.spec);
    }
    catch (ex) {
      // don't make noise when crashreporter is built but not enabled
      if (ex.result != Components.results.NS_ERROR_NOT_INITIALIZED)
        debug(ex);
    }
  },

  /**
   * @param aState is a session state
   * @param aRecentCrashes is the number of consecutive crashes
   * @returns whether a restore page will be needed for the session state
   */
  _needsRestorePage: function sss_needsRestorePage(aState, aRecentCrashes) {
    const SIX_HOURS_IN_MS = 6 * 60 * 60 * 1000;
    
    // don't wrap a single about:sessionrestore page
    let winData = aState.windows || null;
    if (winData && winData.length == 1 && winData[0].tabs &&
        winData[0].tabs.length == 1 && winData[0].tabs[0].entries &&
        winData[0].tabs[0].entries.length == 1 &&
        winData[0].tabs[0].entries[0].url == "about:sessionrestore")
      return false;
    
    // don't automatically restore in Safe Mode
    let XRE = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);
    if (XRE.inSafeMode)
      return true;
    
    let max_resumed_crashes =
      this._prefBranch.getIntPref("sessionstore.max_resumed_crashes");
    let sessionAge = aState.session && aState.session.lastUpdate &&
                     (Date.now() - aState.session.lastUpdate);
    
    return max_resumed_crashes != -1 &&
           (aRecentCrashes > max_resumed_crashes ||
            sessionAge && sessionAge >= SIX_HOURS_IN_MS);
  },

  /**
   * safe eval'ing
   */
  _safeEval: function sss_safeEval(aStr) {
    return Cu.evalInSandbox(aStr, new Cu.Sandbox("about:blank"));
  },

  /**
   * Converts a JavaScript object into a JSON string
   * (see http://www.json.org/ for more information).
   *
   * The inverse operation consists of JSON.parse(JSON_string).
   *
   * @param aJSObject is the object to be converted
   * @returns the object's JSON representation
   */
  _toJSONString: function sss_toJSONString(aJSObject) {
    // XXXzeniko drop the following keys used only for internal bookkeeping:
    //           _tabStillLoading, _hosts, _formDataSaved
    return JSON.stringify(aJSObject);
  },

  _notifyIfAllWindowsRestored: function sss_notifyIfAllWindowsRestored() {
    if (this._restoreCount) {
      this._restoreCount--;
      if (this._restoreCount == 0) {
        // This was the last window restored at startup, notify observers.
        var observerService = Cc["@mozilla.org/observer-service;1"].
                              getService(Ci.nsIObserverService);
        observerService.notifyObservers(null, NOTIFY_WINDOWS_RESTORED, "");
      }
    }
  },

  /**
   * @param aWindow
   *        Window reference
   * @returns whether this window's data is still cached in _statesToRestore
   *          because it's not fully loaded yet
   */
  _isWindowLoaded: function sss_isWindowLoaded(aWindow) {
    return !aWindow.__SS_restoreID;
  },

/* ........ Storage API .............. */

  /**
   * write file to disk
   * @param aFile
   *        nsIFile
   * @param aData
   *        String data
   */
  _writeFile: function sss_writeFile(aFile, aData) {
    // init stream
    var stream = Cc["@mozilla.org/network/safe-file-output-stream;1"].
                 createInstance(Ci.nsIFileOutputStream);
    stream.init(aFile, 0x02 | 0x08 | 0x20, 0600, 0);

    // convert to UTF-8
    var converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                    createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    var convertedData = converter.ConvertFromUnicode(aData);
    convertedData += converter.Finish();

    // write and close stream
    stream.write(convertedData, convertedData.length);
    if (stream instanceof Ci.nsISafeOutputStream) {
      stream.finish();
    } else {
      stream.close();
    }
  }
};

let XPathHelper = {
  // these two hashes should be kept in sync
  namespaceURIs:     { "xhtml": "http://www.w3.org/1999/xhtml" },
  namespacePrefixes: { "http://www.w3.org/1999/xhtml": "xhtml" },

  /**
   * Generates an approximate XPath query to an (X)HTML node
   */
  generate: function sss_xph_generate(aNode) {
    // have we reached the document node already?
    if (!aNode.parentNode)
      return "";
    
    let prefix = this.namespacePrefixes[aNode.namespaceURI] || null;
    let tag = (prefix ? prefix + ":" : "") + aNode.localName;
    
    // stop once we've found a tag with an ID
    if (aNode.id)
      return "//" + tag + "[@id=" + this.quoteArgument(aNode.id) + "]";
    
    // count the number of previous sibling nodes of the same tag
    // (and possible also the same name)
    let count = 0;
    let nName = aNode.name || null;
    for (let n = aNode; (n = n.previousSibling); )
      if (n.localName == aNode.localName && n.namespaceURI == aNode.namespaceURI &&
          (!nName || n.name == nName))
        count++;
    
    // recurse until hitting either the document node or an ID'd node
    return this.generate(aNode.parentNode) + "/" + tag +
           (nName ? "[@name=" + this.quoteArgument(nName) + "]" : "") +
           (count ? "[" + (count + 1) + "]" : "");
  },

  /**
   * Resolves an XPath query generated by XPathHelper.generate
   */
  resolve: function sss_xph_resolve(aDocument, aQuery) {
    let xptype = Ci.nsIDOMXPathResult.FIRST_ORDERED_NODE_TYPE;
    return aDocument.evaluate(aQuery, aDocument, this.resolveNS, xptype, null).singleNodeValue;
  },

  /**
   * Namespace resolver for the above XPath resolver
   */
  resolveNS: function sss_xph_resolveNS(aPrefix) {
    return XPathHelper.namespaceURIs[aPrefix] || null;
  },

  /**
   * @returns a properly quoted string to insert into an XPath query
   */
  quoteArgument: function sss_xph_quoteArgument(aArg) {
    return !/'/.test(aArg) ? "'" + aArg + "'" :
           !/"/.test(aArg) ? '"' + aArg + '"' :
           "concat('" + aArg.replace(/'+/g, "',\"$&\",'") + "')";
  },

  /**
   * @returns an XPath query to all savable form field nodes
   */
  get restorableFormNodes() {
    // for a comprehensive list of all available <INPUT> types see
    // http://mxr.mozilla.org/mozilla-central/search?string=kInputTypeTable
    let ignoreTypes = ["password", "hidden", "button", "image", "submit", "reset"];
    // XXXzeniko work-around until lower-case has been implemented (bug 398389)
    let toLowerCase = '"ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz"';
    let ignore = "not(translate(@type, " + toLowerCase + ")='" +
      ignoreTypes.join("' or translate(@type, " + toLowerCase + ")='") + "')";
    let formNodesXPath = "//textarea|//select|//xhtml:textarea|//xhtml:select|" +
      "//input[" + ignore + "]|//xhtml:input[" + ignore + "]";
    
    delete this.restorableFormNodes;
    return (this.restorableFormNodes = formNodesXPath);
  }
};

function NSGetModule(aComMgr, aFileSpec)
  XPCOMUtils.generateModule([SessionStoreService]);
