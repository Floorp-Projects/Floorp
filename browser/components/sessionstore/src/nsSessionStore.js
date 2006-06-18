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
 *
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Dietrich Ayala <autonome@gmail.com>
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
 * required to return the browser to it's current state. The relevant data is 
 * stored in memory, and is periodically saved to disk in an ini file in the 
 * profile directory. The service is started at first window load, in delayedStartup, and will restore
 * the session from the data received from the nsSessionStartup service.
 */

/* :::::::: Constants and Helpers ::::::::::::::: */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const CID = Components.ID("{5280606b-2510-4fe0-97ef-9b5a22eafe6b}");
const CONTRACT_ID = "@mozilla.org/browser/sessionstore;1";
const CLASS_NAME = "Browser Session Store Service";

const STATE_STOPPED = 0;
const STATE_RUNNING = 1;
const STATE_QUITTING = -1;

const STATE_STOPPED_STR = "stopped";
const STATE_RUNNING_STR = "running";

const PRIVACY_NONE = 0;
const PRIVACY_ENCRYPTED = 1;
const PRIVACY_FULL = 2;

/* :::::::: Pref Defaults :::::::::::::::::::: */

// whether the service is enabled
const DEFAULT_ENABLED = true;

// minimal interval between two save operations (in milliseconds)
const DEFAULT_INTERVAL = 10000;

// maximum number of closed windows remembered
const DEFAULT_MAX_WINDOWS_UNDO = 10;

// maximum number of closed tabs remembered (per window)
const DEFAULT_MAX_TABS_UNDO = 10;

// maximal amount of POSTDATA to be stored (in bytes, -1 = all of it)
const DEFAULT_POSTDATA = 0;

// on which sites to save text data, POSTDATA and cookies
// (0 = everywhere, 1 = unencrypted sites, 2 = nowhere)
const DEFAULT_PRIVACY_LEVEL = PRIVACY_ENCRYPTED;

// resume the current session at startup (otherwise just recover)
const DEFAULT_RESUME_SESSION = false;

// resume the current session at startup just this once
const DEFAULT_RESUME_SESSION_ONCE = false;

// resume the current session at startup if it had previously crashed
const DEFAULT_RESUME_FROM_CRASH = true;

// global notifications observed
const OBSERVING = [
  "domwindowopened", "domwindowclosed",
  "quit-application-requested", "quit-application-granted",
  "quit-application", "browser:purge-session-history"
];

/*
XUL Window properties to (re)store
Restored in restoreDimensions_proxy()
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

function debug(aMsg) {
  aMsg = ("SessionStore: " + aMsg).replace(/\S{80}/g, "$&\n");
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService)
                                     .logStringMessage(aMsg);
}

/* :::::::: The Service ::::::::::::::: */

function SessionStoreService() {
}

SessionStoreService.prototype = {

  // xul:tab attributes to (re)store (extensions might want to hook in here)
  xulAttributes: [],

  // set default load state
  _loadState: STATE_STOPPED,

  // minimal interval between two save operations (in milliseconds)
  _interval: DEFAULT_INTERVAL,

  // time in milliseconds (Date.now()) when the session was last written to file
  _lastSaveTime: 0, 

  // states for all currently opened windows
  _windows: {},

  // storage for Undo Close Window
  _closedWindows: [],

  // in case the last closed window ain't a navigator:browser one
  _lastWindowClosed: null,

  // not-"dirty" windows usually don't need to have their data updated
  _dirtyWindows: {},

  // flag all windows as dirty
  _dirty: false,

/* ........ Global Event Handlers .............. */

  /**
   * Initialize the component
   */
  init: function sss_init(aWindow) {
    debug("store init");
    if (!aWindow || aWindow == null)
      return;

    if (this._loadState == STATE_RUNNING)
      return;

    this._prefBranch = Cc["@mozilla.org/preferences-service;1"].
                       getService(Ci.nsIPrefService).getBranch("browser.");
    this._prefBranch.QueryInterface(Ci.nsIPrefBranch2);

    // if the service is disabled, do not init 
    if (!this._getPref("sessionstore.enabled", DEFAULT_ENABLED))
      return;

    var observerService = Cc["@mozilla.org/observer-service;1"].
                          getService(Ci.nsIObserverService);

    OBSERVING.forEach(function(aTopic) {
      observerService.addObserver(this, aTopic, true);
    }, this);
    
    // get interval from prefs - used often, so caching/observing instead of fetching on-demand
    this._interval = this._getPref("sessionstore.interval", DEFAULT_INTERVAL);
    this._prefBranch.addObserver("sessionstore.interval", this, true);
    
    // observe prefs changes so we can modify stored data to match
    this._prefBranch.addObserver("sessionstore.max_windows_undo", this, true);
    this._prefBranch.addObserver("sessionstore.max_tabs_undo", this, true);

    // get file references
    var dirService = Cc["@mozilla.org/file/directory_service;1"].
                     getService(Ci.nsIProperties);
    this._sessionFile = dirService.get("ProfD", Ci.nsILocalFile);
    this._sessionFileBackup = this._sessionFile.clone();
    this._sessionFile.append("sessionstore.ini");
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
        this._initialState = IniObjectSerializer.decode(iniString);
        // set bool detecting crash
        this._lastSessionCrashed =
          this._initialState.Session && this._initialState.Session.state &&
          this._initialState.Session.state == STATE_RUNNING_STR;
        
        // restore the features of the first window from localstore.rdf
        WINDOW_ATTRIBUTES.forEach(function(aAttr) {
          delete this._initialState.Window[0][aAttr];
        }, this);
        delete this._initialState.Window[0].hidden;
      }
      catch (ex) { debug("The session file is invalid: " + ex); } // invalid .INI file - nothing can be restored
    }
    
    // if last session crashed, backup the session
    if (this._lastSessionCrashed) {
      try {
        this._writeFile(this._getSessionFile(true), iniString);
      }
      catch (ex) { } // nothing else we can do here
    }

    // As this is called at delayedStartup, restoration must be initiated here
    var windowLoad = function(self) {
      self.onLoad(this);
    }
    aWindow.setTimeout(windowLoad, 0, this);
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
    var observerService = Cc["@mozilla.org/observer-service;1"].
                          getService(Ci.nsIObserverService);

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
      this._dirty = false;
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
      this._closedWindows = [];
      this._forEachBrowserWindow(function(aWindow) {
        Array.forEach(aWindow.getBrowser().browsers, function(aBrowser) {
          delete aBrowser.parentNode.__SS_data;
        });
      });
      this._clearDisk();
      // give the tabbrowsers a chance to clear their histories first
      var win = this._getMostRecentBrowserWindow();
      if (win)
        win.setTimeout(function() { _this.saveState(true); }, 0);
      else
        this.saveState(true);
      break;
    case "nsPref:changed": // catch pref changes
      switch (aData) {
      // if the user decreases the max number of windows they want preserved
      // update our internal state to match that max
      case "sessionstore.max_windows_undo":
        this._closedWindows.splice(this._getPref("sessionstore.max_windows_undo", DEFAULT_MAX_WINDOWS_UNDO));
        break;
      // if the user decreases the max number of closed tabs they want
      // preserved update our internal states to match that max
      case "sessionstore.max_tabs_undo":
        var ix;
        for (ix in this._windows) {
          this._windows[ix]._closedTabs.splice(this._getPref("sessionstore.max_tabs_undo", DEFAULT_MAX_TABS_UNDO));
        }
        break;
      case "sessionstore.interval":
        this._interval = this._getPref("sessionstore.interval", this._interval);
        // reset timer and save
        if (this._saveTimer) {
          this._saveTimer.cancel();
          this._saveTimer = null;
        }
        this.saveStateDelayed(null, -1);
      }
      break;
    case "timer-callback": // timer call back for delayed saving
      this._saveTimer = null;
      this.saveStateDelayed(null, -1);
      break;
    }
  },

/* ........ Window Event Handlers .............. */

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
    var _this = this;

    // ignore non-browser windows
    if (aWindow.document.documentElement.getAttribute("windowtype") != "navigator:browser")
      return;
    
    // assign it a unique identifier (timestamp)
    aWindow.__SSi = "window" + Date.now();

    // and create its data object
    this._windows[aWindow.__SSi] = { Tab: [], selected: 0, _closedTabs: [] };
    
    // perform additional initialization when the first window is loading
    if (this._loadState == STATE_STOPPED) {
      this._loadState = STATE_RUNNING;
      this._lastSaveTime = Date.now();
      
      // don't save during the first five seconds
      // (until most of the pages have been restored)
      this.saveStateDelayed(aWindow, 10000);

      // restore a crashed session resp. resume the last session if requested
      if (this._initialState) {
        // make sure that the restored tabs are first in the window
        this._initialState._firstTabs = true;
        this.restoreWindow(aWindow, this._initialState, this._isCmdLineEmpty(aWindow));
        delete this._initialState;
      }
      
      // restart any interrupted downloads
      aWindow.setTimeout(function(){ _this.retryDownloads(aWindow); }, 0);
    }
    
    var tabbrowser = aWindow.getBrowser();
    var tabpanels = tabbrowser.mPanelContainer;
    
    // add tab change listeners to all already existing tabs
    for (var i = 0; i < tabpanels.childNodes.length; i++) {
      this.onTabAdd(aWindow, tabpanels.childNodes[i], true);
    }
    // notification of tab add/remove/selection
    // http://developer.mozilla.org/en/docs/Extension_Code_Snippets:Tabbed_Browser
    // - to be replaced with events from bug 322898
    tabpanels.addEventListener("DOMNodeInserted", function(aEvent) {
      _this.onTabAdd(aEvent.currentTarget.ownerDocument.defaultView, aEvent.target);
      }, false);
    tabpanels.addEventListener("DOMNodeRemoved", function(aEvent) {
      _this.onTabRemove(aEvent.currentTarget.ownerDocument.defaultView, aEvent.target);
      }, false);
    tabpanels.addEventListener("select", function(aEvent) {
      _this.onTabSelect(aEvent.currentTarget.ownerDocument.defaultView, aEvent.currentTarget);
      }, false);
    tabbrowser.addEventListener("TabClose", function(aEvent) {
      _this.onTabClose(aEvent.currentTarget.ownerDocument.defaultView, aEvent.originalTarget);
      }, false);
  },

  /**
   * On window close...
   * - remove event listeners from tabs
   * - save all window data
   * @param aWindow
   *        Window reference
   */
  onClose: function sss_onClose(aWindow) {
    if (aWindow.document.documentElement.getAttribute("windowtype") != "navigator:browser") {
      return;
    }
    
    var tabbrowser = aWindow.getBrowser();
    var tabpanels = tabbrowser.mPanelContainer;
    
    for (var i = 0; i < tabpanels.childNodes.length; i++) {
      this.onTabRemove(aWindow, tabpanels.childNodes[i], true);
    }
    
    if (this._loadState == STATE_RUNNING) { // window not closed during a regular shut-down 
      // update all window data for a last time
      this._collectWindowData(aWindow);
      
      // preserve this window's data (in case it was the last navigator:browser)
      this._lastWindowClosed = this._windows[aWindow.__SSi];
      this._lastWindowClosed.title = aWindow.content.document.title;
      this._updateCookies([this._lastWindowClosed]);
      
      // add the window to the list of recently closed windows
      this._closedWindows.unshift(this._lastWindowClosed);
      this._closedWindows.splice(this._getPref("sessionstore.max_windows_undo", DEFAULT_MAX_WINDOWS_UNDO));
      
      // clear this window from the list
      delete this._windows[aWindow.__SSi];
      
      // save the state without this window to disk
      this.saveStateDelayed();
    }
    
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
    var _this = this;
    aPanel.addEventListener("load", function(aEvent) {
      _this.onTabLoad(aEvent.currentTarget.ownerDocument.defaultView, aEvent.currentTarget, aEvent);
      }, true);
    aPanel.addEventListener("pageshow", function(aEvent) {
      _this.onTabLoad(aEvent.currentTarget.ownerDocument.defaultView, aEvent.currentTarget, aEvent);
      }, true);
    aPanel.addEventListener("input", function(aEvent) {
      _this.onTabInput(this.ownerDocument.defaultView, this, aEvent);
      }, true);
    aPanel.addEventListener("DOMAutoComplete", function(aEvent) {
      _this.onTabInput(this.ownerDocument.defaultView, this, aEvent);
      }, true);
    
    if (!aNoNotification) {
      this.saveStateDelayed(aWindow);
    }
  },

  /**
   * remove listeners for a tab
   * zeniko: unify with onTabClose as soon as the required events are available
   * @param aWindow
   *        Window reference
   * @param aPanel
   *        TabPanel reference
   * @param aNoNotification
   *        bool Do not save state if we're updating an existing tab
   */
  onTabRemove: function sss_onTabRemove(aWindow, aPanel, aNoNotification) {
    aPanel.removeEventListener("load", this.onTabLoad_proxy, true);
    aPanel.removeEventListener("pageshow", this.onTabLoad_proxy, true);
    aPanel.removeEventListener("input", this.onTabInput_proxy, true);
    aPanel.removeEventListener("DOMAutoComplete", this.onTabInput_proxy, true);
    
    delete aPanel.__SS_data;
    delete aPanel.__SS_text;
    
    if (!aNoNotification) {
      this.saveStateDelayed(aWindow);
    }
  },

  /**
   * When a tab closes, collect it's properties
   * @param aWindow
   *        Window reference
   * @param aTab
   *        TabPanel reference
   */
  onTabClose: function sss_onTabClose(aWindow, aTab) {
    // don't update our internal state if we don't have to
    if (this._getPref("sessionstore.max_tabs_undo", DEFAULT_MAX_TABS_UNDO) == 0) {
      return;
    }
    
    // make sure that the tab related data is up-to-date
    this._saveWindowHistory(aWindow);
    this._updateTextAndScrollData(aWindow);
    
    this._windows[aWindow.__SSi]._closedTabs.unshift({
      state: this._windows[aWindow.__SSi].Tab[aTab._tPos],
      title: aTab.getAttribute("label"),
      pos: aTab._tPos
    });
    this._windows[aWindow.__SSi]._closedTabs.splice(this._getPref("sessionstore.max_tabs_undo", DEFAULT_MAX_TABS_UNDO));
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
    delete aPanel.__SS_text;
    this.saveStateDelayed(aWindow);
  },

  /**
   * Called when a tabpanel sends the "input" notification 
   * stores textarea data
   * @param aWindow
   *        Window reference
   * @param aPanel
   *        TabPanel reference
   * @param aEvent
   *        Event obj
   */
  onTabInput: function sss_onTabInput(aWindow, aPanel, aEvent) {
    if (this._saveTextData(aPanel, XPCNativeWrapper(aEvent.originalTarget))) {
      this.saveStateDelayed(aWindow, 3000);
    }
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
    }
  },

/* ........ nsISessionStore API .............. */

// This API is terribly rough. TODO:
// * figure out what functionality we really need
// * figure out how to name the functions

// * get/set the (opaque) state of all windows

  get opaqueState() {
    return this._getCurrentState();
  },

  set opaqueState(aState) {
    var window = this._getMostRecentBrowserWindow();
    if (!window) {
      this._openWindowWithState(aState);
      return;
    }
    
    // close all other browser windows
    this._forEachBrowserWindow(function(aWindow) {
      if (aWindow != window) {
        aWindow.close();
      }
    });

    // restore to the given state
    this.restoreWindow(window, aState, true);
  },

  getOpaqueWindowState: function sss_getOpaqueWindowState(aWindow) {
    return this._getWindowState(aWindow);
  },

  setOpaqueWindowState: function sss_setOpaqueWindowState(aWindow, aState) {
    this.restoreWindow(aWindow, aState, true);
  },

// * allow to reopen closed windows
// * get/set the opaque state of a closed window (for persisting it over sessions)

  get closedWindowCount() {
    return this._closedWindows.length;
  },

  closedWindowNameAt: function sss_closedWindowNameAt(aIx) {
    return aIx in this._closedWindows ? this._closedWindows[aIx].title : null;
  },

  get closedWindowData() {
    return IniObjectSerializer.encode(this._closedWindows);
  },

  set closedWindowData(aData) {
    this._closedWindows = IniObjectSerializer.decode(aData);
  },

  undoCloseWindow: function sss_undoCloseWindow(aIx) {
    if (aIx in this._closedWindows) {
      this._openWindowWithState({ Window: this._closedWindows.splice(aIx, 1) });
    }
    else {
      Components.returnCode = -1; //zeniko: or should we rather fail silently?
    }
  },

// * allow to reopen closed tabs
// * get/set the opaque state of a closed tab (for persisting it over sessions)

  getClosedTabCount: function sss_getClosedTabCount(aWindow) {
    return this._windows[aWindow.__SSi]._closedTabs.length;
  },

  closedTabNameAt: function sss_closedTabNameAt(aWindow, aIx) {
    var tabs = this._windows[aWindow.__SSi]._closedTabs;
    
    return aIx in tabs ? tabs[aIx].title : null;
  },

  getClosedTabData: function sss_getClosedTabDataAt(aWindow) {
    return IniObjectSerializer.encode(this._windows[aWindow.__SSi]._closedTabs);
  },

  setClosedTabData: function sss_setClosedTabDataAt(aWindow, aData) {
    this._windows[aWindow.__SSi]._closedTabs = IniObjectSerializer.decode(aData);
  },

  undoCloseTab: function sss_undoCloseWindow(aWindow, aIx) {
    var tabs = this._windows[aWindow.__SSi]._closedTabs;
    
    if (aIx in tabs) {
      var browser = aWindow.getBrowser();
      var tabData = tabs.splice(aIx, 1);
      tabData._tab = browser.addTab();
      
      // restore the tab's position
      browser.moveTabTo(tabData._tab, tabData[0].pos);

      // restore the tab's state
      aWindow.setTimeout(this.restoreHistory_proxy, 0, tabData, 1, 0, 0);
    }
    else {
      Components.returnCode = -1; //zeniko: or should we rather fail silently?
    }
  },

// * get/set persistent properties on individual tabs or windows (for extensions)

  getWindowValue: function sss_getWindowValue(aWindow, aKey) {
    if (aWindow.__SSi) {
      var data = this._windows[aWindow.__SSi].ExtData || {};
      return data[aKey] || "";
    }
    else {
      Components.returnCode = -1; //zeniko: or should we rather fail silently?
    }
  },

  setWindowValue: function sss_setWindowValue(aWindow, aKey, aStringValue) {
    if (aWindow.__SSi) {
      if (!this._windows[aWindow.__SSi].ExtData) {
        this._windows[aWindow.__SSi].ExtData = {};
      }
      this._windows[aWindow.__SSi].ExtData[aKey] = aStringValue;
      this.saveStateDelayed(aWindow);
    }
    else {
      Components.returnCode = -1; //zeniko: or should we rather fail silently?
    }
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
    this.saveStateDelayed(aTop.ownerDocument.defaultView);
  },

  persistTabAttribute: function sss_persistTabAttribute(aName) {
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
    var browsers = tabbrowser.browsers;
    var tabs = this._windows[aWindow.__SSi].Tab = [];
    this._windows[aWindow.__SSi].selected = 0;
    
    for (var i = 0; i < browsers.length; i++) {
      var tabData = { Entry: [], index: 0 };
      
      var browser = browsers[i];
      if (!browser || !browser.currentURI) {
        // can happen when calling this function right after .addTab()
        tabs.push(tabData);
        continue;
      }
      var history = null;
      
      try {
        history = browser.sessionHistory;
      }
      catch (ex) { } // this could happen if we catch a tab during (de)initialization
      
      if (history && browser.parentNode.__SS_data && browser.parentNode.__SS_data.Entry[history.index]) {
        tabData = browser.parentNode.__SS_data;
        tabData.index = history.index + 1;
      }
      else if (history && history.count > 0) {
        for (var j = 0; j < history.count; j++) {
          tabData.Entry.push(this._serializeHistoryEntry(history.getEntryAtIndex(j, false)));
        }
        tabData.index = history.index + 1;
        
        browser.parentNode.__SS_data = tabData;
      }
      else {
        tabData.Entry[0] = { url: browser.currentURI.spec };
        tabData.index = 1;
      }
      tabData.zoom = browser.markupDocumentViewer.textZoom;
      
      var disallow = CAPABILITIES.filter(function(aCapability) {
        return !browser.docShell["allow" + aCapability];
      });
      tabData.disallow = disallow.join(",");
      
      var _this = this;
      var xulattr = Array.filter(tabbrowser.mTabs[i].attributes, function(aAttr) {
        return (_this.xulAttributes.indexOf(aAttr.name) > -1);
      }).map(function(aAttr) {
        return aAttr.name + "=" + encodeURI(aAttr.value);
      });
      tabData.xultab = xulattr.join(" ");
      
      tabData.ExtData = tabbrowser.mTabs[i].__SS_extdata || null;
      
      tabs.push(tabData);
      
      if (browser == tabbrowser.selectedBrowser) {
        this._windows[aWindow.__SSi].selected = i + 1;
      }
    }
  },

  /**
   * Get an object that is a serialized representation of a History entry
   * Used for data storage
   * @param aEntry
   *        nsISHEntry instance
   * @returns object
   */
  _serializeHistoryEntry: function sss_serializeHistoryEntry(aEntry) {
    var entry = { url: aEntry.URI.spec, Child: [] };
    
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
    if (cacheKey && cacheKey instanceof Ci.nsISupportsPRUint32) {
      entry.cacheKey = cacheKey.data;
    }
    entry.ID = aEntry.ID;
    
    var x = {}, y = {};
    aEntry.getScrollPosition(x, y);
    entry.scroll = x.value + "," + y.value;
    
    try {
      var prefPostdata = this._getPref("sessionstore.postdata", DEFAULT_POSTDATA);
      if (prefPostdata && aEntry.postData && this._checkPrivacyLevel(aEntry.URI.schemeIs("https"))) {
        aEntry.postData.QueryInterface(Ci.nsISeekableStream).
                        seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);
        var stream = Cc["@mozilla.org/scriptableinputstream;1"].
                     createInstance(Ci.nsIScriptableInputStream);
        stream.init(aEntry.postData);
        // Warning: LiveHTTPHeaders crashes around here!
        var postdata = stream.read(stream.available());
        if (prefPostdata == -1 || postdata.replace(/^(Content-.*\r\n)+(\r\n)*/, "").length <= prefPostdata) {
          entry.postdata = postdata;
        }
        //zeniko: should we close the stream here (since we never really opened it)?
      }
    }
    catch (ex) { debug(ex); } // POSTDATA is tricky - especially since some extensions don't get it right
    
    if (!(aEntry instanceof Ci.nsISHContainer)) {
      return entry;
    }
    
    for (var i = 0; i < aEntry.childCount; i++) {
      var child = aEntry.GetChildAt(i);
      if (child) {
        entry.Child.push(this._serializeHistoryEntry(child));
      }
      else { // to maintain the correct frame order, insert a dummy entry 
        entry.Child.push({ url: "about:blank" });
      }
    }
    
    return entry;
  },

  /**
   * Updates the current document's cache of user entered text data
   * @param aPanel
   *        TabPanel reference
   * @param aTextarea
   * @returns bool
   */
  _saveTextData: function sss_saveTextData(aPanel, aTextarea) {
    var id = aTextarea.id ? "#" + aTextarea.id : aTextarea.name;
    if (!id
      || !(aTextarea instanceof Ci.nsIDOMHTMLTextAreaElement 
      || aTextarea instanceof Ci.nsIDOMHTMLInputElement)) {
      return false; // nothing to save
    }
    
    // determine the frame we're in and encode it into the textarea's ID
    var content = aTextarea.ownerDocument.defaultView;
    while (content != content.top) {
      var frames = content.parent.frames;
      for (var i = 0; i < frames.length && frames[i] != content; i++);
      id = i + "|" + id;
      content = content.parent;
    }
    
    if (!aPanel.__SS_text) {
      aPanel.__SS_text = {};
    }
    aPanel.__SS_text[id] = aTextarea.value;
    
    return true;
  },

  /**
   * go through all frames and store the current scroll positions
   * and innerHTML content of WYSIWYG editors
   * @param aWindow
   *        Window reference
   */
  _updateTextAndScrollData: function sss_updateTextAndScrollData(aWindow) {
    var _this = this;
    function updateRecursively(aContent, aData) {
      for (var i = 0; i < aContent.frames.length; i++) {
        if (aData.Child && aData.Child[i]) {
          updateRecursively(aContent.frames[i], aData.Child[i]);
        }
      }
      // designMode is undefined e.g. for XUL documents (as about:config)
      var isHTTPS = _this._getURIFromString((aContent.parent || aContent).
                                        document.location.href).schemeIs("https");
      if ((aContent.document.designMode || "") == "on" && _this._checkPrivacyLevel(isHTTPS)) {
        if (aData.innerHTML == undefined) {
          // we get no "input" events from iframes - listen for keypress here
          aContent.addEventListener("keypress", function(aEvent) { _this.saveStateDelayed(aWindow, 3000); }, true);
        }
        aData.innerHTML = aContent.document.body.innerHTML;
      }
      aData.scroll = aContent.scrollX + "," + aContent.scrollY;
    }
    
    Array.forEach(aWindow.getBrowser().browsers, function(aBrowser, aIx) {
      try {
        var tabData = this._windows[aWindow.__SSi].Tab[aIx];
        
        var text = [];
        if (aBrowser.parentNode.__SS_text && this._checkPrivacyLevel(aBrowser.currentURI.schemeIs("https"))) {
          for (var id in aBrowser.parentNode.__SS_text) {
            text.push(id + "=" + encodeURI(aBrowser.parentNode.__SS_text[id]));
          }
        }
        if (aBrowser.currentURI.spec == "about:config") {
          text = ["#textbox=" + encodeURI(aBrowser.contentDocument.getElementById("textbox").value)];
        }
        tabData.text = text.join(" ");
        
        updateRecursively(XPCNativeWrapper(aBrowser.contentWindow), tabData.Entry[tabData.index - 1]);
      }
      catch (ex) { debug(ex); } // get as much data as possible, ignore failures (might succeed the next time)
    }, this);
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
      if (aEntry.Child) {
        aEntry.Child.forEach(extractHosts);
      }
    }
    
    this._windows[aWindow.__SSi].Tab.forEach(function(aTabData) { aTabData.Entry.forEach(extractHosts); });
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
    for (var i = 0; i < aWindows.length; i++) {
      aWindows[i].Cookies = { count: 0 };
    }
    
    var _this = this;
    while (cookiesEnum.hasMoreElements()) {
      var cookie = cookiesEnum.getNext().QueryInterface(Ci.nsICookie2);
      if (cookie.isSession && cookie.host) {
        var url = "", value = "";
        aWindows.forEach(function(aWindow) {
          if (aWindow._hosts && aWindow._hosts[cookie.rawHost]) {
            // make sure to construct URL and value only once per cookie
            if (!url) {
              var url = "http" + (cookie.isSecure ? "s" : "") + "://" + cookie.host + (cookie.path || "").replace(/^(?!\/)/, "/");
              if (_this._checkPrivacyLevel(cookie.isSecure)) {
                value = (cookie.name || "name") + "=" + (cookie.value || "") + ";";
                value += cookie.isDomain ? "domain=" + cookie.rawHost + ";" : "";
                value += cookie.path ? "path=" + cookie.path + ";" : "";
                value += cookie.isSecure ? "secure;" : "";
              }
            }
            if (value) {
              // in order to not unnecessarily bloat the session file,
              // all window cookies are saved into one JS object
              var cookies = aWindow.Cookies;
              cookies["domain" + ++cookies.count] = url;
              cookies["value" + cookies.count] = value;
            }
          }
        });
      }
    }
    
    // don't include empty cookie sections
    for (i = 0; i < aWindows.length; i++) {
      if (aWindows[i].Cookies.count == 0) {
        aWindows[i].Cookies = null;
      }
    }
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
    
    winData.hidden = WINDOW_HIDEABLE_FEATURES.filter(function(aItem) {
      return aWindow[aItem] && !aWindow[aItem].visible;
    }).join(",");
    
    winData.sidebar = aWindow.document.getElementById("sidebar-box").getAttribute("sidebarcommand");
  },

  /**
   * serialize session data as Ini-formatted string
   * @returns string
   */
  _getCurrentState: function sss_getCurrentState() {
    var activeWindow = this._getMostRecentBrowserWindow();
    
    if (this._loadState == STATE_RUNNING) {
      // update the data for all windows with activities since the last save operation
      this._forEachBrowserWindow(function(aWindow) {
        if (this._dirty|| this._dirtyWindows[aWindow.__SSi] || aWindow == activeWindow) {
          this._collectWindowData(aWindow);
        }
        else { // always update the window features (whose change alone never triggers a save operation)
          this._updateWindowFeatures(aWindow);
        }
      }, this);
      this._dirtyWindows = [];
      this._dirty = false;
    }
    
    // collect the data for all windows
    var total = [], windows = [];
    var ix;
    for (ix in this._windows) {
      total.push(this._windows[ix]);
      windows.push(ix);
    }
    this._updateCookies(total);
    
    // make sure that the current window is restored first
    var ix = activeWindow ? windows.indexOf(activeWindow.__SSi || "") : -1;
    if (ix > 0) {
      total.unshift(total.splice(ix, 1)[0]);
    }

    // if no browser window remains open, return the state of the last closed window
    if (total.length == 0 && this._lastWindowClosed) {
      total.push(this._lastWindowClosed);
    }
    
    return IniObjectSerializer.encode({ Window: total });
  },

  /**
   * serialize session data for a window 
   * @param aWindow
   *        Window reference
   * @returns string
   */
  _getWindowState: function sss_getWindowState(aWindow) {
    if (this._loadState == STATE_RUNNING) {
      this._collectWindowData(aWindow);
    }
    
    var total = [this._windows[aWindow.__SSi]];
    this._updateCookies(total);
    
    return IniObjectSerializer.encode({ Window: total });
  },

  _collectWindowData: function sss_collectWindowData(aWindow) {
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
   *        Ini formatted string, or object
   * @param aOverwriteTabs
   *        bool overwrite existing tabs w/ new ones
   */
  restoreWindow: function sss_restoreWindow(aWindow, aState, aOverwriteTabs) {
    try {
      var root = typeof aState == "string" ? IniObjectSerializer.decode(aState) : aState;
      if (!root.Window[0]) {
        return; // nothing to restore
      }
    }
    catch (ex) { // invalid .INI file - don't restore anything 
      debug(ex);
      return;
    }
    
    // open new windows for all further window entries of a multi-window session
    for (var w = 1; w < root.Window.length; w++) {
      this._openWindowWithState({ Window: [root.Window[w]], opener: aWindow });
    }
    
    var winData = root.Window[0];
    if (!winData.Tab) {
      winData.Tab = [];
    }
    
    var tabbrowser = aWindow.getBrowser();
    var openTabCount = aOverwriteTabs ? tabbrowser.browsers.length : -1;
    var newTabCount = winData.Tab.length;
    
    for (var t = 0; t < newTabCount; t++) {
      winData.Tab[t]._tab = t < openTabCount ? tabbrowser.mTabs[t] : tabbrowser.addTab();
      // when resuming at startup: add additionally requested pages to the end
      if (!aOverwriteTabs && root._firstTabs) {
        tabbrowser.moveTabTo(winData.Tab[t]._tab, t);
      }
    }

    // when overwriting tabs, remove all superflous ones
    for (t = openTabCount - 1; t >= newTabCount; t--) {
      tabbrowser.removeTab(tabbrowser.mTabs[t]);
    }
    
    if (aOverwriteTabs) {
      this.restoreWindowFeatures(aWindow, winData, root.opener || null);
    }
    if (winData.Cookies) {
      this.restoreCookies(winData.Cookies);
    }
    if (winData.ExtData) {
      if (!this._windows[aWindow.__SSi].ExtData) {
        this._windows[aWindow.__SSi].ExtData = {}
      }
      for (var key in winData.ExtData) {
        this._windows[aWindow.__SSi].ExtData[key] = winData.ExtData[key];
      }
    }
    
    var restoreHistoryFunc = function(self) {
      self.restoreHistory_proxy(aWindow, winData.Tab, (aOverwriteTabs ?
      (parseInt(winData.selected) || 1) : 0), 0, 0);
    };
    aWindow.setTimeout(restoreHistoryFunc, 0, this);
  },

  /**
   * Manage history restoration for a window
   * @param aTabs
   *        Array of tab data
   * @param aCurrentTabs
   *        Array of tab references
   * @param aSelectTab
   *        Index of selected tab
   * @param aCount
   *        Counter for number of times delaying b/c browser or history aren't ready
   */
  restoreHistory_proxy: function sss_restoreHistory_proxy(aWindow, aTabs, aSelectTab, aIx, aCount) {
    var tabbrowser = aWindow.getBrowser();
    
    // make sure that all browsers and their histories are available
    // - if one's not, resume this check in 100ms (repeat at most 10 times)
    for (var t = aIx; t < aTabs.length; t++) {
      try {
        if (!tabbrowser.getBrowserForTab(aTabs[t]._tab).webNavigation.sessionHistory) {
          throw new Error();
        }
      }
      catch (ex) { // in case browser or history aren't ready yet 
        if (aCount < 10) {
          var restoreHistoryFunc = function(self) {
            self.restoreHistory_proxy(aWindow, aTabs, aSelectTab, aIx, aCount + 1);
          }
          aWindow.setTimeout(restoreHistoryFunc, 100, this);
          return;
        }
      }
    }
    
    // mark the tabs as loading (at this point about:blank
    // has completed loading in all tabs, so it won't interfere)
    for (t = 0; t < aTabs.length; t++) {
      var tab = aTabs[t]._tab;
      tab.setAttribute("busy", "true");
      tabbrowser.updateIcon(tab);
      tabbrowser.setTabTitleLoading(tab);
    }
    
    // make sure to restore the selected tab first (if any)
    if (aSelectTab-- && aTabs[aSelectTab]) {
        aTabs.unshift(aTabs.splice(aSelectTab, 1)[0]);
        tabbrowser.selectedTab = aTabs[0]._tab;
    }
    
    this.restoreHistory(aWindow, aTabs);
  },

  /**
   * Restory history for a window
   * @param aWindow
   *        Window reference
   * @param aTabs
   *        Array of tab data
   * @param aCurrentTabs
   *        Array of tab references
   * @param aSelectTab
   *        Index of selected tab
   */
  restoreHistory: function sss_restoreHistory(aWindow, aTabs, aIdMap) {
    var _this = this;
    while (aTabs.length > 0 && (!aTabs[0]._tab || !aTabs[0]._tab.parentNode)) {
      aTabs.shift(); // this tab got removed before being completely restored
    }
    if (aTabs.length == 0) {
      return; // no more tabs to restore
    }
    
    var tabData = aTabs.shift();

    // helper hash for ensuring unique frame IDs
    var idMap = { used: {} };
    
    var tab = tabData._tab;
    var browser = aWindow.getBrowser().getBrowserForTab(tab);
    var history = browser.webNavigation.sessionHistory;
    
    if (history.count > 0) {
      history.PurgeHistory(history.count);
    }
    history.QueryInterface(Ci.nsISHistoryInternal);
    
    if (!tabData.Entry) {
      tabData.Entry = [];
    }
    if (tabData.ExtData) {
      tab.__SS_extdata = tabData.ExtData;
    }
    
    browser.markupDocumentViewer.textZoom = parseFloat(tabData.zoom || 1);
    
    for (var i = 0; i < tabData.Entry.length; i++) {
      history.addEntry(this._deserializeHistoryEntry(tabData.Entry[i], idMap), true);
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
      tabData.xultab.split(" ").forEach(function(aAttr) {
        if (/^([^\s=]+)=(.*)/.test(aAttr)) {
          tab.setAttribute(RegExp.$1, decodeURI(RegExp.$2));
        }
      });
    }
    
    // notify the tabbrowser that the tab chrome has been restored
    var event = aWindow.document.createEvent("Events");
    event.initEvent("SSTabRestoring", true, false);
    tab.dispatchEvent(event);
    
    var activeIndex = (tabData.index || tabData.Entry.length) - 1;
    try {
      browser.webNavigation.gotoIndex(activeIndex);
    }
    catch (ex) { } // ignore an invalid tabData.index
    
    // restore those aspects of the currently active documents
    // which are not preserved in the plain history entries
    // (mainly scroll state and text data)
    browser.__SS_restore_data = tabData.Entry[activeIndex] || {};
    browser.__SS_restore_text = tabData.text || "";
    browser.__SS_restore_tab = tab;
    browser.__SS_restore = this.restoreDocument_proxy;
    browser.addEventListener("load", browser.__SS_restore, true);
    
    aWindow.setTimeout(function(){ _this.restoreHistory(aWindow, aTabs, aIdMap); }, 0);
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
    shEntry.setIsSubFrame(aEntry.subframe || false);
    shEntry.loadType = Ci.nsIDocShellLoadInfo.loadHistory;
    
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
    
    var scrollPos = (aEntry.scroll || "0,0").split(",");
    scrollPos = [parseInt(scrollPos[0]) || 0, parseInt(scrollPos[1]) || 0];
    shEntry.setScrollPosition(scrollPos[0], scrollPos[1]);
    
    if (aEntry.postdata) {
      var stream = Cc["@mozilla.org/io/string-input-stream;1"].
                   createInstance(Ci.nsIStringInputStream);
      stream.setData(aEntry.postdata, -1);
      shEntry.postData = stream;
    }
    
    if (aEntry.Child && shEntry instanceof Ci.nsISHContainer) {
      for (var i = 0; i < aEntry.Child.length; i++) {
        shEntry.AddChild(this._deserializeHistoryEntry(aEntry.Child[i], aIdMap), i);
      }
    }
    
    return shEntry;
  },

  /**
   * Restore properties to a loaded document
   */
  restoreDocument_proxy: function sss_restoreDocument_proxy(aEvent) {
    // wait for the top frame to be loaded completely
    if (!aEvent || !aEvent.originalTarget || !aEvent.originalTarget.defaultView || aEvent.originalTarget.defaultView != aEvent.originalTarget.defaultView.top) {
      return;
    }
    
    var textArray = this.__SS_restore_text ? this.__SS_restore_text.split(" ") : [];
    function restoreTextData(aContent) {
      textArray.forEach(function(aEntry) {
        if (/^((?:\d+\|)*)(#?)([^\s=]+)=(.*)$/.test(aEntry) && (!RegExp.$1 || RegExp.$1 == aPrefix)) {
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
    
    function restoreTextDataAndScrolling(aContent, aData, aPrefix) {
      restoreTextData(aContent, aPrefix);
      if (aData.innerHTML) {
        aContent.setTimeout(function(aHTML) { if (this.document.designMode == "on") { this.document.body.innerHTML = aHTML; } }, 0, aData.innerHTML);
      }
      if (aData.scroll && /(\d+),(\d+)/.test(aData.scroll)) {
        aContent.scrollTo(RegExp.$1, RegExp.$2);
      }
      for (var i = 0; i < aContent.frames.length; i++) {
        if (aData.Child && aData.Child[i]) {
          restoreTextDataAndScrolling(aContent.frames[i], aData.Child[i], i + "|" + aPrefix);
        }
      }
    }
    
    var content = XPCNativeWrapper(aEvent.originalTarget).defaultView;
    if (this.currentURI.spec == "about:config") { //zeniko: why ever this doesn't work with an XPCNativeWrapper...
      content = aEvent.originalTarget.defaultView;
    }
    restoreTextDataAndScrolling(content, this.__SS_restore_data, "");
    
    // notify the tabbrowser that this document has been completely restored
    var event = this.ownerDocument.createEvent("Events");
    event.initEvent("SSTabRestored", true, false);
    this.__SS_restore_tab.dispatchEvent(event);
    
    this.removeEventListener("load", this.__SS_restore, true);
    delete this.__SS_restore_data;
    delete this.__SS_restore_text;
    delete this.__SS_restore_tab;
    delete this.__SS_restore;
  },

  /**
   * Restore visibility and dimension features to a window
   * @param aWindow
   *        Window reference
   * @param aWinData
   *        Object containing session data for the window
   * @param aOpener
   *        Opening window, for refocusing
   */
  restoreWindowFeatures: function sss_restoreWindowFeatures(aWindow, aWinData, aOpener) {
    var hidden = (aWinData.hidden)?aWinData.hidden.split(","):[];
    WINDOW_HIDEABLE_FEATURES.forEach(function(aItem) {
      aWindow[aItem].visible = hidden.indexOf(aItem) == -1;
    });
    
    var _this = this;
    aWindow.setTimeout(function() {
      _this.restoreDimensions_proxy.apply(_this, [aWindow, aOpener, aWinData.width || 0, 
        aWinData.height || 0, "screenX" in aWinData ? aWinData.screenX : NaN,
        "screenY" in aWinData ? aWinData.screenY : NaN,
        aWinData.sizemode || "", aWinData.sidebar || ""]);
    }, 0);
  },

  /**
   * Restore a window's dimensions
   * @param aOpener
   *        Opening window, for refocusing
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
  restoreDimensions_proxy: function sss_restoreDimensions_proxy(aWindow, aOpener, aWidth, aHeight, aLeft, aTop, aSizeMode, aSidebar) {
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
    // we might want to re-focus the window which created this one
    if (aOpener) {
      aOpener.focus();
    }
  },

  /**
   * Restores cookies to cookie service
   * @param aCookies
   *        Array of cookie data
   */
  restoreCookies: function sss_restoreCookies(aCookies) {
    var cookieService = Cc["@mozilla.org/cookieService;1"].
                        getService(Ci.nsICookieService);
    var ioService = Cc["@mozilla.org/network/io-service;1"].
                    getService(Ci.nsIIOService);
    
    for (var i = 1; i <= aCookies.count; i++) {
      try {
        cookieService.setCookieString(ioService.newURI(aCookies["domain" + i], null, null), null, aCookies["value" + i] + "expires=0;", null);
      }
      catch (ex) { debug(ex); } // don't let a single cookie stop recovering (might happen if a user tried to edit the session file)
    }
  },

  /**
   * Restart incomplete downloads
   * @param aWindow
   *        Window reference
   */
  retryDownloads: function sss_retryDownloads(aWindow) {
    var downloadManager = Cc["@mozilla.org/download-manager;1"].
                          getService(Ci.nsIDownloadManager);
    var rdfService = Cc["@mozilla.org/rdf/rdf-service;1"].
                     getService(Ci.nsIRDFService);
    var ioService = Cc["@mozilla.org/network/io-service;1"].
                    getService(Ci.nsIIOService);
    
    var rdfContainer = Cc["@mozilla.org/rdf/container;1"].
                       createInstance(Ci.nsIRDFContainer);
    var datasource = downloadManager.datasource;
    
    try {
      rdfContainer.Init(datasource, rdfService.GetResource("NC:DownloadsRoot"));
    }
    catch (ex) { // missing downloads datasource
      return;
    }
    
    var downloads = rdfContainer.GetElements();
    
    // iterate through all downloads currently available in the RDF store
    // and restart the ones which were in progress before the crash
    while (downloads.hasMoreElements()) {
      var download = downloads.getNext().QueryInterface(Ci.nsIRDFResource);
      
      var node = datasource.GetTarget(rdfService.GetResource(download.Value), rdfService.GetResource("http://home.netscape.com/NC-rdf#DownloadState"), true);
      if (node) {
        node.QueryInterface(Ci.nsIRDFInt);
      }
      if (!node || node.Value != Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING) {
        continue;
      }
      
      node = datasource.GetTarget(rdfService.GetResource(download.Value), rdfService.GetResource("http://home.netscape.com/NC-rdf#URL"), true);
      node.QueryInterface(Ci.nsIRDFResource);
      
      var url = node.Value;
      
      node = datasource.GetTarget(rdfService.GetResource(download.Value), rdfService.GetResource("http://home.netscape.com/NC-rdf#File"), true);
      node.QueryInterface(Ci.nsIRDFResource);
      
      var linkChecker = Cc["@mozilla.org/network/urichecker;1"].
                        createInstance(Ci.nsIURIChecker);
      
      linkChecker.init(ioService.newURI(url, null, null));
      linkChecker.loadFlags = Ci.nsIRequest.LOAD_BACKGROUND;
      linkChecker.asyncCheck(new AutoDownloader(url, node.Value, aWindow), null);
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
    // interval until the next disk operation is allowed
    var minimalDelay = this._lastSaveTime + this._interval - Date.now();

    if (aWindow) {
      this._dirtyWindows[aWindow.__SSi] = true;
    }

    if (!this._saveTimer) {
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
    this._dirty = aUpdateAll;
    this._writeFile(this._getSessionFile(), [
      "; This file is automatically generated",
      "; Do not edit while Firefox is running",
      "; The file encoding is UTF-8",
      "[Session]",
      "state=" + (this._loadState == STATE_RUNNING ? STATE_RUNNING_STR : STATE_STOPPED_STR),
      this._getCurrentState(),
      ""
    ].join("\n").replace(/\n\[/g, "\n$&"));
    this._lastSaveTime = Date.now();
  },

  /**
   * delete session datafile and backup
   */
  _clearDisk: function sss_clearDisk() {
    var file = this._getSessionFile();

    if (file.exists()) {
      try {
        file.remove(false);
      }
      catch (ex) { dump(ex + '\n'); } // couldn't remove the file - what now?
    }

    if (!this._lastSessionCrashed)
      return;

    try {
      this._getSessionFile(true).remove(false);
    }
    catch (ex) { dump(ex + '\n'); } // couldn't remove the file - what now?
  },

  /**
   * get session datafile (or its backup)
   * @returns nsIFile 
   */
  _getSessionFile: function sss_getSessionFile(aBackup) {
    return aBackup ? this._sessionFileBackup : this._sessionFile;
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
    
    //zeniko: why isn't it possible to set the window's dimensions here (as feature)?
    var window = Cc["@mozilla.org/embedcomp/window-watcher;1"].
                 getService(Ci.nsIWindowWatcher).
                 openWindow(null, this._getPref("chromeURL", null), "_blank", "chrome,dialog=no,all", null);
    
    window.__SS_state = aState;
    var _this = this;
    window.addEventListener("load", function(aEvent) {
      aEvent.currentTarget.removeEventListener("load", arguments.callee, true);
      _this.restoreWindow(aEvent.currentTarget, aEvent.currentTarget.__SS_state, true, true);
      delete aEvent.currentTarget.__SS_state;
    }, true);
  },

  /**
   * Whether or not to resume session, if not recovering from a crash
   * Returns true if:
   * - pref is set to always resume sessions
   * - pref is set to resume session once
   * - user configured startup page to be the last-visited page
   * @returns bool
   */
  _doResumeSession: function sss_doResumeSession() {
    return this._getPref("sessionstore.resume_session", DEFAULT_RESUME_SESSION)
      || this._getPref("sessionstore.resume_session_once", DEFAULT_RESUME_SESSION_ONCE)
      || this._getPref("startup.page", 1) == 2;
  },

  /**
   * whether the user wants to load any other page at startup
   * (except the homepage) - needed for determining whether to overwrite the current tabs
   * @returns bool
   */
  _isCmdLineEmpty: function sss_isCmdLineEmpty(aWindow) {
    if (!aWindow.arguments) {
      return true;
    }
    
    var homepage = null;
    switch (this._getPref("startup.page", 1)) {
    case 0:
      homepage = "about:blank";
      break;
    case 1:
      try {
        homepage = this._prefBranch.getComplexValue("startup.homepage", Ci.nsIPrefLocalizedString).data;
      }
      catch (ex) {
        homepage = this._getPref("startup.homepage", "");
      }
      break;
    case 2:
      homepage = Cc["@mozilla.org/browser/global-history;2"].
                 getService(Ci.nsIBrowserHistory).lastPageVisited;
      break;
    }
    
    for (var i = 0; i < aWindow.arguments.length; i++) {
      var url = aWindow.arguments[i].split("\n")[0];
      if (!url || url == homepage) {
        aWindow.arguments.splice(i--, 1);
      }
    }
    
    return (aWindow.arguments.length == 0);
  },

  /**
   * don't save sensitive data if the user doesn't want to
   * (distinguishes between encrypted and non-encrypted sites)
   * @param aIsHTTPS
   *        Bool is encrypted
   * @returns bool
   */
  _checkPrivacyLevel: function sss_checkPrivacyLevel(aIsHTTPS) {
    return this._getPref("sessionstore.privacy_level", DEFAULT_PRIVACY_LEVEL) < (aIsHTTPS ? PRIVACY_ENCRYPTED : PRIVACY_FULL);
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
   * Convenience method to get localized string bundles
   * @param aURI
   * @returns nsIStringBundle
   */
  _getStringBundle: function sss_getStringBundle(aURI) {
     var bundleService = Cc["@mozilla.org/intl/stringbundle;1"].
                         getService(Ci.nsIStringBundleService);
     var appLocale = Cc["@mozilla.org/intl/nslocaleservice;1"].
                     getService(Ci.nsILocaleService).getApplicationLocale();
     return bundleService.createBundle(aURI, appLocale);
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

/* ........ Storage API .............. */

  /**
   * basic pref reader
   * @param aName
   * @param aDefault
   * @param aUseRootBranch
   */
  _getPref: function sss_getPref(aName, aDefault) {
    var pb = this._prefBranch;
    try {
      switch (pb.getPrefType(aName)) {
      case pb.PREF_STRING:
        return pb.getCharPref(aName);
      case pb.PREF_BOOL:
        return pb.getBoolPref(aName);
      case pb.PREF_INT:
        return pb.getIntPref(aName);
      default:
        return aDefault;
      }
    }
    catch(ex) {
      return aDefault;
    }
  },

  /**
   * reads a file into a string
   * @param aFile
   *        nsIFile
   * @returns string
   */
  _readFile: function sss_readFile(aFile) {
    try {
      var stream = Cc["@mozilla.org/network/file-input-stream;1"].
                   createInstance(Ci.nsIFileInputStream);
      stream.init(aFile, 0x01, 0, 0);
      var cvstream = Cc["@mozilla.org/intl/converter-input-stream;1"].
                     createInstance(Ci.nsIConverterInputStream);
      cvstream.init(stream, "UTF-8", 1024, Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);
      
      var content = "";
      var data = {};
      while (cvstream.readString(4096, data)) {
        content += data.value;
      }
      cvstream.close();
      
      return content.replace(/\r\n?/g, "\n");
    }
    catch (ex) { } // inexisting file?
    
    return null;
  },

  /**
   * write file to disk
   * @param aFile
   *        nsIFile
   * @param aData
   *        String data
   */
  _writeFile: function sss_writeFile(aFile, aData) {
    // save the file in the current thread
    // (making sure we don't get killed at shutdown)
    (new FileWriter(aFile, aData)).run();
    return;
  },

/* ........ QueryInterface .............. */

  QueryInterface: function(aIID) {
    if (!aIID.equals(Ci.nsISupports) && !aIID.equals(Ci.nsIObserver) && 
      !aIID.equals(Ci.nsISupportsWeakReference) && 
      !aIID.equals(Ci.nsISessionStore)) {
      Components.returnCode = Cr.NS_ERROR_NO_INTERFACE;
      return null;
    }
    
    return this;
  }
};

/* :::::::::: FileWriter :::::::::::::: */

// a threaded file writer for somewhat better performance in the UI thread
function FileWriter(aFile, aData) {
  this._file = aFile;
  this._data = aData;
}

FileWriter.prototype = {
  run: function FileWriter_run() {
    // init stream
    var stream = Cc["@mozilla.org/network/safe-file-output-stream;1"].
                 createInstance(Ci.nsIFileOutputStream);
    stream.init(this._file, 0x02 | 0x08 | 0x20, 0600, 0);

    // convert to UTF-8
    var converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                    createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    var convertedData = converter.ConvertFromUnicode(this._data);
    convertedData += converter.Finish();

    // write and close stream
    stream.write(convertedData, convertedData.length);
    if (stream instanceof Ci.nsISafeOutputStream) {
      stream.finish();
    } else {
      stream.close();
    }
  },
};

/* :::::::::: Asynchronous File Downloader :::::::::::::: */

function AutoDownloader(aURL, aFilename, aWindow) {
   this._URL = aURL;
   this._filename = aFilename;
   this._window = aWindow;
}

AutoDownloader.prototype = {
  onStartRequest: function(aRequest, aContext) { },
  onStopRequest: function(aRequest, aContext, aStatus) {
    if (Components.isSuccessCode(aStatus)) {
      var file =
        Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
      file.initWithPath(this._filename);
      if (file.exists()) {
        file.remove(false);
      }
      
      this._window.saveURL(this._URL, this._filename, null, true, true, null);
    }
  }
};

/* :::::::: File Format Converter ::::::::::::::: */

/**
 * Sessions are serialized to and restored from .INI files
 * 
 * The serialization can store nested JS objects and arrays. Each object or array
 * gets a session header containing the "path" to it (using dots as name separators).
 * Arrays are stored as a list of objects where the path name is the array name plus
 * the object's index (e.g. [Window2.Cookies] is the Cookies object of the second entry
 * of the Window array).
 * 
 * The .INI format used here is for performance and convenience reasons somewhat restricted:
 * * files must be stored in a Unicode compatible encoding (such as UTF-8)
 * * section headers and key names are case-sensitive
 * * dots in section names have special meaning (separating the names in the object hierarchy)
 * * numbers can have some special meaning as well (see below)
 * * keys could occur in the "root" section (i.e. before the first section header)
 * 
 * Despite these restrictions, these files should be quite interoperable with other .INI
 * parsers since we're quite conservative in what we emit and pretty tolerant in what we
 * accept (except for the encoding and the case sensitivity issues - which could be
 * remedied by encoding all values containing non US-ASCII characters and by requesting
 * all keys to be lower-cased (and enforcing this when restoring).
 * 
 * Implementation details you have to be aware of:
 * * empty (string) values aren't stored at all
 * * keys beginning with an underscore are ignored
 * * Arrays are stored with index-base 1 (NOT: 0!)
 * * Array lengths are stored implicitely
 * * true and false are (re)stored as boolean values
 * * positive integers are returned as Number (all other numbers as String)
 * * string values can contain all possible characters
 *   (they are automatically URI encoded/decoded should that be necessary)
 */
var IniObjectSerializer = {
  encode: function(aObj, aPrefix) {
    aPrefix = aPrefix ? aPrefix + "." : "";
    
    var ini = [], iniChildren = [];
    
    for (var key in aObj) {
      if (!key || /[;\[\]=]/.test(key)) {
        debug("Ignoring invalid key name: '" + key + "'!");
        continue;
      }
      else if (key.charAt(0) == "_") { // ignore this key
        continue;
      }
      
      var value = aObj[key];
      if (typeof value == "boolean" || typeof value == "number") {
        ini.push(key + "=" + value);
      }
      else if (typeof value == "string" && value) {
        ini.push(key + "=" + (/^\s|[%\t\r\n;]|\s$/.test(value) ? encodeURI(value).replace(/;/g, "%3B") : value));
      }
      else if (value instanceof Array) {
        for (var i = 0; i < value.length; i++) {
          if (value[i] instanceof Object) {
            iniChildren.push("[" + aPrefix + key + (i + 1) + "]");
            iniChildren.push(this.encode(value[i], aPrefix + key + (i + 1)));
          }
        }
      }
      else if (typeof value == "object" && value) {
        iniChildren.push("[" + aPrefix + key + "]");
        iniChildren.push(this.encode(value, aPrefix + key));
      }
    }
    
    return ini.concat(iniChildren).join("\n");
  },

  decode: function(aIniString) {
    var rootObject = {};
    var obj = rootObject;
    var lines = aIniString.split("\n");
    
    for (var i = 0; i < lines.length; i++) {
      var line = lines[i].replace(/;.*/, "");
      try {
        if (line.charAt(0) == "[") {
          obj = this._getObjForHeader(rootObject, line);
        }
        else if (/\S/.test(line)) { // ignore blank lines
          this._setValueForLine(obj, line);
        }
      }
      catch (ex) {
        throw new Error("Error at line " + (i + 1) + ": " + ex.description);
      }
    }
    
    return rootObject;
  },

  // Object which results after traversing the path indicated in the section header
  _getObjForHeader: function(aObj, aLine) {
    var names = aLine.split("]")[0].substr(1).split(".");
    
    for (var i = 0; i < names.length; i++) {
      var name = names[i];
      if (!names[i]) {
        throw new Error("Invalid header: [" + names.join(".") + "]!");
      }
      if (/(\d+)$/.test(names[i])) {
        names[i] = names[i].slice(0, -RegExp.$1.length);
        var ix = parseInt(RegExp.$1) - 1;
        aObj = aObj[names[i]] = aObj[names[i]] || [];
        aObj = aObj[ix] = aObj[ix] || {};
      }
      else {
        aObj = aObj[names[i]] = aObj[names[i]] || {};
      }
    }
    
    return aObj;
  },

  _setValueForLine: function(aObj, aLine) {
    var ix = aLine.indexOf("=");
    if (ix < 1) {
      throw new Error("Invalid entry: " + aLine + "!");
    }
    
    var key = this._trim(aLine.substr(0, ix));
    var value = this._trim(aLine.substr(ix + 1));
    if (value == "true" || value == "false") {
      value = (value == "true");
    }
    else if (/^\d+$/.test(value)) {
      value = parseInt(value);
    }
    else if (value.indexOf("%") > -1) {
      value = decodeURI(value.replace(/%3B/gi, ";"));
    }
    
    aObj[key] = value;
  },

  _trim: function(aString) {
    return aString.replace(/^\s+|\s+$/g, "");
  }
};

/* :::::::: Service Registration & Initialization ::::::::::::::: */

/* ........ nsIModule .............. */

const SessionStoreModule = {

  getClassObject: function(aCompMgr, aCID, aIID) {
    if (aCID.equals(CID)) {
      return SessionStoreFactory;
    }
    
    Components.returnCode = Cr.NS_ERROR_NOT_REGISTERED;
    return null;
  },

  registerSelf: function(aCompMgr, aFileSpec, aLocation, aType) {
    aCompMgr.QueryInterface(Ci.nsIComponentRegistrar);
    aCompMgr.registerFactoryLocation(CID, CLASS_NAME, CONTRACT_ID, aFileSpec, aLocation, aType);

    var catMan = Cc["@mozilla.org/categorymanager;1"].
                 getService(Ci.nsICategoryManager);
    catMan.addCategoryEntry("app-startup", CLASS_NAME, "service," + CONTRACT_ID, true, true);
  },

  unregisterSelf: function(aCompMgr, aLocation, aType) {
    aCompMgr.QueryInterface(Ci.nsIComponentRegistrar);
    aCompMgr.unregisterFactoryLocation(CID, aLocation);

    var catMan = Cc["@mozilla.org/categorymanager;1"].
                 getService(Ci.nsICategoryManager);
    catMan.deleteCategoryEntry( "app-startup", "service," + CONTRACT_ID, true);
  },

  canUnload: function(aCompMgr) {
    return true;
  }
}

/* ........ nsIFactory .............. */

const SessionStoreFactory = {

  createInstance: function(aOuter, aIID) {
    if (aOuter != null) {
      Components.returnCode = Cr.NS_ERROR_NO_AGGREGATION;
      return null;
    }
    
    return (new SessionStoreService()).QueryInterface(aIID);
  },

  lockFactory: function(aLock) { },

  QueryInterface: function(aIID) {
    if (!aIID.equals(Ci.nsISupports) && !aIID.equals(Ci.nsIModule) &&
        !aIID.equals(Ci.nsIFactory) && !aIID.equals(Ci.nsISessionStore)) {
      Components.returnCode = Cr.NS_ERROR_NO_INTERFACE;
      return null;
    }
    
    return this;
  }
};

function NSGetModule(aComMgr, aFileSpec) {
  return SessionStoreModule;
}
