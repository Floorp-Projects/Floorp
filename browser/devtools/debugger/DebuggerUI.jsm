/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const DBG_XUL = "chrome://browser/content/devtools/debugger.xul";
const DBG_STRINGS_URI = "chrome://browser/locale/devtools/debugger.properties";
const CHROME_DEBUGGER_PROFILE_NAME = "-chrome-debugger";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this,
  "DebuggerServer", "resource://gre/modules/devtools/dbg-server.jsm");

XPCOMUtils.defineLazyModuleGetter(this,
  "Services", "resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["DebuggerUI"];

/**
 * Provides a simple mechanism of managing debugger instances.
 *
 * @param nsIDOMWindow aWindow
 *        The chrome window for which the DebuggerUI instance is created.
 */
this.DebuggerUI = function DebuggerUI(aWindow) {
  this.chromeWindow = aWindow;
  this.listenToTabs();
};

DebuggerUI.prototype = {
  /**
   * Update the status of tool's menuitems and buttons when
   * the user switches tabs.
   */
  listenToTabs: function() {
    let win = this.chromeWindow;
    let tabs = win.gBrowser.tabContainer;

    let bound_refreshCommand = this.refreshCommand.bind(this);
    tabs.addEventListener("TabSelect", bound_refreshCommand, true);

    win.addEventListener("unload", function onClose(aEvent) {
      win.removeEventListener("unload", onClose, false);
      tabs.removeEventListener("TabSelect", bound_refreshCommand, true);
    }, false);
  },

  /**
   * Called by the DebuggerPane to update the Debugger toggle switches with the
   * debugger state.
   */
  refreshCommand: function() {
    let scriptDebugger = this.getDebugger();
    let command = this.chromeWindow.document.getElementById("Tools:Debugger");
    let selectedTab = this.chromeWindow.gBrowser.selectedTab;

    if (scriptDebugger && scriptDebugger.ownerTab === selectedTab) {
      command.setAttribute("checked", "true");
    } else {
      command.setAttribute("checked", "false");
    }
  },

  /**
   * Starts a debugger for the current tab, or stops it if already started.
   *
   * @return DebuggerPane | null
   *         The script debugger instance if it's started, null if stopped.
   */
  toggleDebugger: function() {
    let scriptDebugger = this.findDebugger();
    let selectedTab = this.chromeWindow.gBrowser.selectedTab;

    if (scriptDebugger) {
      scriptDebugger.close();
      return null;
    }
    return new DebuggerPane(this, selectedTab);
  },

  /**
   * Starts a remote debugger in a new window, or stops it if already started.
   *
   * @return RemoteDebuggerWindow | null
   *         The remote debugger instance if it's started, null if stopped.
   */
  toggleRemoteDebugger: function() {
    let remoteDebugger = this.getRemoteDebugger();

    if (remoteDebugger) {
      remoteDebugger.close();
      return null;
    }
    return new RemoteDebuggerWindow(this);
  },

  /**
   * Starts a chrome debugger in a new process, or stops it if already started.
   *
   * @return ChromeDebuggerProcess | null
   *         The chrome debugger instance if it's started, null if stopped.
   */
  toggleChromeDebugger: function(aOnClose, aOnRun) {
    let chromeDebugger = this.getChromeDebugger();

    if (chromeDebugger) {
      chromeDebugger.close();
      return null;
    }
    return new ChromeDebuggerProcess(this, aOnClose, aOnRun);
  },

  /**
   * Gets the current script debugger from any open window.
   *
   * @return DebuggerPane | null
   *         The script debugger instance if it exists, null otherwise.
   */
  findDebugger: function() {
    let enumerator = Services.wm.getEnumerator("navigator:browser");
    while (enumerator.hasMoreElements()) {
      let chromeWindow = enumerator.getNext().QueryInterface(Ci.nsIDOMWindow);
      let scriptDebugger = chromeWindow.DebuggerUI.getDebugger();
      if (scriptDebugger) {
        return scriptDebugger;
      }
    }
    return null;
  },

  /**
   * Get the current script debugger.
   *
   * @return DebuggerPane | null
   *         The script debugger instance if it exists, null otherwise.
   */
  getDebugger: function() {
    return '_scriptDebugger' in this ? this._scriptDebugger : null;
  },

  /**
   * Get the remote debugger for the current chrome window.
   *
   * @return RemoteDebuggerWindow | null
   *         The remote debugger instance if it exists, null otherwise.
   */
  getRemoteDebugger: function() {
    return '_remoteDebugger' in this ? this._remoteDebugger : null;
  },

  /**
   * Get the chrome debugger for the current firefox instance.
   *
   * @return ChromeDebuggerProcess | null
   *         The chrome debugger instance if it exists, null otherwise.
   */
  getChromeDebugger: function() {
    return '_chromeDebugger' in this ? this._chromeDebugger : null;
  }
};

/**
 * Creates a pane that will host the debugger.
 *
 * @param DebuggerUI aDebuggerUI
 *        The parent instance creating the new debugger.
 * @param XULElement aTab
 *        The tab in which to create the debugger.
 */
this.DebuggerPane = function DebuggerPane(aDebuggerUI, aTab) {
  this.globalUI = aDebuggerUI;
  this._win = aDebuggerUI.chromeWindow;
  this._tab = aTab;

  this.close = this.close.bind(this);
  this._initServer();
  this._create();
}

DebuggerPane.prototype = {
  /**
   * Initializes the debugger server.
   */
  _initServer: function() {
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }
  },

  /**
   * Creates and initializes the widgets containing the debugger UI.
   */
  _create: function() {
    this.globalUI._scriptDebugger = this;

    let gBrowser = this._win.gBrowser;
    let ownerDocument = gBrowser.parentNode.ownerDocument;

    this._splitter = ownerDocument.createElement("splitter");
    this._splitter.setAttribute("class", "devtools-horizontal-splitter");

    this._frame = ownerDocument.createElement("iframe");

    this._nbox = gBrowser.getNotificationBox(this._tab.linkedBrowser);
    this._nbox.appendChild(this._splitter);
    this._nbox.appendChild(this._frame);

    let self = this;

    this._frame.addEventListener("Debugger:Loaded", function dbgLoaded() {
      self._frame.removeEventListener("Debugger:Loaded", dbgLoaded, true);
      self._frame.addEventListener("Debugger:Unloaded", self.close, true);

      // Bind shortcuts for accessing the breakpoint methods in the debugger.
      let bkp = self.contentWindow.DebuggerController.Breakpoints;
      self.addBreakpoint = bkp.addBreakpoint;
      self.removeBreakpoint = bkp.removeBreakpoint;
      self.getBreakpoint = bkp.getBreakpoint;
      self.breakpoints = bkp.store;
    }, true);

    this._frame.setAttribute("src", DBG_XUL);
    this.globalUI.refreshCommand();
  },

  /**
   * Closes the debugger, removing child nodes and event listeners.
   *
   * @param function aCloseCallback
   *        Clients can pass a close callback to be notified when
   *        the panel successfully closes.
   */
  close: function(aCloseCallback) {
    if (!this.globalUI) {
      return;
    }
    delete this.globalUI._scriptDebugger;

    // This method is also used as an event handler, so only
    // use aCloseCallback if it's a function.
    if (typeof aCloseCallback == "function") {
      let frame = this._frame;
      frame.addEventListener("unload", function onUnload() {
        frame.removeEventListener("unload", onUnload, true);
        aCloseCallback();
      }, true)
    }

    this._frame.removeEventListener("Debugger:Unloaded", this.close, true);
    this._nbox.removeChild(this._splitter);
    this._nbox.removeChild(this._frame);

    this._splitter = null;
    this._frame = null;
    this._nbox = null;
    this._win = null;
    this._tab = null;

    // Remove shortcuts for accessing the breakpoint methods in the debugger.
    delete this.addBreakpoint;
    delete this.removeBreakpoint;
    delete this.getBreakpoint;
    delete this.breakpoints;

    this.globalUI.refreshCommand();
    this.globalUI = null;
  },

  /**
   * Gets the chrome window owning this debugger instance.
   * @return XULWindow
   */
  get ownerWindow() {
    return this._win;
  },

  /**
   * Gets the tab owning this debugger instance.
   * @return XULElement
   */
  get ownerTab() {
    return this._tab;
  },

  /**
   * Gets the debugger content window.
   * @return nsIDOMWindow
   */
  get contentWindow() {
    return this._frame ? this._frame.contentWindow : null;
  }
};

/**
 * Creates a window that will host a remote debugger.
 *
 * @param DebuggerUI aDebuggerUI
 *        The parent instance creating the new debugger.
 */
this.RemoteDebuggerWindow = function RemoteDebuggerWindow(aDebuggerUI) {
  this.globalUI = aDebuggerUI;
  this._win = aDebuggerUI.chromeWindow;

  this.close = this.close.bind(this);
  this._create();
}

RemoteDebuggerWindow.prototype = {
  /**
   * Creates and initializes the widgets containing the remote debugger UI.
   */
  _create: function() {
    this.globalUI._remoteDebugger = this;

    this._dbgwin = this.globalUI.chromeWindow.open(DBG_XUL,
      L10N.getStr("remoteDebuggerWindowTitle"), "chrome,dependent,resizable");

    let self = this;

    this._dbgwin.addEventListener("Debugger:Loaded", function dbgLoaded() {
      self._dbgwin.removeEventListener("Debugger:Loaded", dbgLoaded, true);
      self._dbgwin.addEventListener("Debugger:Unloaded", self.close, true);

      // Bind shortcuts for accessing the breakpoint methods in the debugger.
      let bkp = self.contentWindow.DebuggerController.Breakpoints;
      self.addBreakpoint = bkp.addBreakpoint;
      self.removeBreakpoint = bkp.removeBreakpoint;
      self.getBreakpoint = bkp.getBreakpoint;
      self.breakpoints = bkp.store;
    }, true);

    this._dbgwin._remoteFlag = true;
  },

  /**
   * Closes the remote debugger, along with the parent window if necessary.
   */
  close: function() {
    if (!this.globalUI) {
      return;
    }
    delete this.globalUI._remoteDebugger;

    this._dbgwin.removeEventListener("Debugger:Unloaded", this.close, true);
    this._dbgwin.close();
    this._dbgwin = null;
    this._win = null;

    // Remove shortcuts for accessing the breakpoint methods in the debugger.
    delete this.addBreakpoint;
    delete this.removeBreakpoint;
    delete this.getBreakpoint;
    delete this.breakpoints;

    this.globalUI = null;
  },

  /**
   * Gets the chrome window owning this debugger instance.
   * @return XULWindow
   */
  get ownerWindow() {
    return this._win;
  },

  /**
   * Gets the remote debugger content window.
   * @return nsIDOMWindow.
   */
  get contentWindow() {
    return this._dbgwin;
  }
};

/**
 * Creates a process that will hold a chrome debugger.
 *
 * @param DebuggerUI aDebuggerUI
 *        The parent instance creating the new debugger.
 * @param function aOnClose
 *        Optional, a function called when the process exits.
 * @param function aOnRun
 *        Optional, a function called when the process starts running.
 */
this.ChromeDebuggerProcess = function ChromeDebuggerProcess(aDebuggerUI, aOnClose, aOnRun) {
  this.globalUI = aDebuggerUI;
  this._win = aDebuggerUI.chromeWindow;
  this._closeCallback = aOnClose;
  this._runCallback = aOnRun;

  this._initServer();
  this._initProfile();
  this._create();
}

ChromeDebuggerProcess.prototype = {
  /**
   * Initializes the debugger server.
   */
  _initServer: function() {
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }
    DebuggerServer.openListener(Prefs.chromeDebuggingPort);
  },

  /**
   * Initializes a profile for the remote debugger process.
   */
  _initProfile: function() {
    let profileService = Cc["@mozilla.org/toolkit/profile-service;1"]
      .createInstance(Ci.nsIToolkitProfileService);

    let profileName;
    try {
      // Attempt to get the required chrome debugging profile name string.
      profileName = profileService.selectedProfile.name + CHROME_DEBUGGER_PROFILE_NAME;
    } catch (e) {
      // Requested profile string could not be retrieved.
      profileName = CHROME_DEBUGGER_PROFILE_NAME;
      Cu.reportError(e);
    }

    let profileObject;
    try {
      // Attempt to get the required chrome debugging profile toolkit object.
      profileObject = profileService.getProfileByName(profileName);

      // The profile exists but the corresponding folder may have been deleted.
      var enumerator = Services.dirsvc.get("ProfD", Ci.nsIFile).parent.directoryEntries;
      while (enumerator.hasMoreElements()) {
        let profileDir = enumerator.getNext().QueryInterface(Ci.nsIFile);
        if (profileDir.leafName.contains(profileName)) {
          // Requested profile was found and the folder exists.
          this._dbgProfile = profileObject;
          return;
        }
      }
      // Requested profile was found but the folder was deleted. Cleanup needed.
      profileObject.remove(true);
    } catch (e) {
      // Requested profile object was not found.
      Cu.reportError(e);
    }

    // Create a new chrome debugging profile.
    this._dbgProfile = profileService.createProfile(null, null, profileName);
    profileService.flush();
  },

  /**
   * Creates and initializes the profile & process for the remote debugger.
   */
  _create: function() {
    this.globalUI._chromeDebugger = this;

    let file = Services.dirsvc.get("XREExeF", Ci.nsIFile);

    dumpn("Initializing chrome debugging process");
    let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
    process.init(file);

    let args = [
      "-no-remote", "-P", this._dbgProfile.name,
      "-chrome", DBG_XUL];

    dumpn("Running chrome debugging process");
    process.runwAsync(args, args.length, { observe: this.close.bind(this) });
    this._dbgProcess = process;

    if (typeof this._runCallback == "function") {
      this._runCallback.call({}, this);
    }
  },

  /**
   * Closes the remote debugger, removing the profile and killing the process.
   */
  close: function() {
    dumpn("Closing chrome debugging process");
    if (!this.globalUI) {
      dumpn("globalUI is missing");
      return;
    }
    delete this.globalUI._chromeDebugger;

    if (this._dbgProcess.isRunning) {
      dumpn("Killing chrome debugging process...");
      this._dbgProcess.kill();
    }
    dumpn("...done.");
    if (typeof this._closeCallback == "function") {
      this._closeCallback.call({}, this);
    }

    this._dbgProcess = null;
    this._dbgProfile = null;
    this._win = null;

    this.globalUI = null;
  }
};

/**
 * Localization convenience methods.
 */
let L10N = {
  /**
   * L10N shortcut function.
   *
   * @param string aName
   * @return string
   */
  getStr: function(aName) {
    return this.stringBundle.GetStringFromName(aName);
  }
};

XPCOMUtils.defineLazyGetter(L10N, "stringBundle", function() {
  return Services.strings.createBundle(DBG_STRINGS_URI);
});

/**
 * Shortcuts for accessing various debugger preferences.
 */
let Prefs = {};

/**
 * Gets the preferred default remote browser debugging port.
 * @return number
 */
XPCOMUtils.defineLazyGetter(Prefs, "chromeDebuggingPort", function() {
  return Services.prefs.getIntPref("devtools.debugger.chrome-debugging-port");
});

/**
 * Helper method for debugging.
 * @param string
 */
function dumpn(str) {
  if (wantLogging) {
    dump("DBG-FRONTEND: " + str + "\n");
  }
}

let wantLogging = Services.prefs.getBoolPref("devtools.debugger.log");
