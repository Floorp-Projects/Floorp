/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const DBG_XUL = "chrome://browser/content/debugger.xul";
const DBG_STRINGS_URI = "chrome://browser/locale/devtools/debugger.properties";
const REMOTE_PROFILE_NAME = "_remote-debug";

Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let EXPORTED_SYMBOLS = ["DebuggerUI"];

/**
 * Provides a simple mechanism of managing debugger instances per tab.
 *
 * @param nsIDOMWindow aWindow
 *        The chrome window for which the DebuggerUI instance is created.
 */
function DebuggerUI(aWindow) {
  this.chromeWindow = aWindow;
}

DebuggerUI.prototype = {
  /**
   * Called by the DebuggerPane to update the Debugger toggle switches with the
   * debugger state.
   */
  refreshCommand: function DUI_refreshCommand() {
    let selectedTab = this.chromeWindow.getBrowser().selectedTab;
    let command = this.chromeWindow.document.getElementById("Tools:Debugger");

    if (this.getDebugger(selectedTab) != null) {
      command.setAttribute("checked", "true");
    } else {
      command.removeAttribute("checked");
    }
  },

  /**
   * Starts a debugger for the current tab, or stops it if already started.
   * @return DebuggerPane if the debugger is started, null if it's stopped.
   */
  toggleDebugger: function DUI_toggleDebugger() {
    let tab = this.chromeWindow.gBrowser.selectedTab;

    if (tab._scriptDebugger) {
      tab._scriptDebugger.close();
      return null;
    }
    return new DebuggerPane(this, tab);
  },

  /**
   * Starts a remote debugger in a new window, or stops it if already started.
   * @return RemoteDebuggerWindow if the debugger is started, null if stopped.
   */
  toggleRemoteDebugger: function DUI_toggleRemoteDebugger() {
    let win = this.chromeWindow;

    if (win._remoteDebugger) {
      win._remoteDebugger.close();
      return null;
    }
    return new RemoteDebuggerWindow(this);
  },

  /**
   * Starts a chrome debugger in a new process, or stops it if already started.
   * @return ChromeDebuggerProcess if the debugger is started, null if stopped.
   */
  toggleChromeDebugger: function DUI_toggleChromeDebugger(aOnClose, aOnRun) {
    let win = this.chromeWindow;

    if (win._chromeDebugger) {
      win._chromeDebugger.close();
      return null;
    }
    return new ChromeDebuggerProcess(win, aOnClose, aOnRun, true);
  },

  /**
   * Get the debugger for a specified tab.
   * @return DebuggerPane if a debugger exists for the tab, null otherwise.
   */
  getDebugger: function DUI_getDebugger(aTab) {
    return '_scriptDebugger' in aTab ? aTab._scriptDebugger : null;
  },

  /**
   * Get the remote debugger for the current chrome window.
   * @return RemoteDebuggerWindow if a remote debugger exists, null otherwise.
   */
  getRemoteDebugger: function DUI_getRemoteDebugger() {
    let win = this.chromeWindow;
    return '_remoteDebugger' in win ? win._remoteDebugger : null;
  },

  /**
   * Get the chrome debugger for the current firefox instance.
   * @return ChromeDebuggerProcess if a chrome debugger exists, null otherwise.
   */
  getChromeDebugger: function DUI_getChromeDebugger() {
    let win = this.chromeWindow;
    return '_chromeDebugger' in win ? win._chromeDebugger : null;
  },

  /**
   * Get the preferences associated with the debugger frontend.
   * @return object
   */
  get preferences() {
    return DebuggerPreferences;
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
function DebuggerPane(aDebuggerUI, aTab) {
  this._globalUI = aDebuggerUI;
  this._tab = aTab;

  this._initServer();
  this._create();
}

DebuggerPane.prototype = {

  /**
   * Initializes the debugger server.
   */
  _initServer: function DP__initServer() {
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }
  },

  /**
   * Creates and initializes the widgets containing the debugger UI.
   */
  _create: function DP__create() {
    this._tab._scriptDebugger = this;

    let gBrowser = this._tab.linkedBrowser.getTabBrowser();
    let ownerDocument = gBrowser.parentNode.ownerDocument;

    this._splitter = ownerDocument.createElement("splitter");
    this._splitter.setAttribute("class", "devtools-horizontal-splitter");

    this._frame = ownerDocument.createElement("iframe");
    this._frame.height = DebuggerPreferences.height;

    this._nbox = gBrowser.getNotificationBox(this._tab.linkedBrowser);
    this._nbox.appendChild(this._splitter);
    this._nbox.appendChild(this._frame);

    this.close = this.close.bind(this);
    let self = this;

    this._frame.addEventListener("Debugger:Loaded", function dbgLoaded() {
      self._frame.removeEventListener("Debugger:Loaded", dbgLoaded, true);
      self._frame.addEventListener("Debugger:Close", self.close, true);
      self._frame.addEventListener("unload", self.close, true);

      // Bind shortcuts for accessing the breakpoint methods in the debugger.
      let bkp = self.contentWindow.DebuggerController.Breakpoints;
      self.addBreakpoint = bkp.addBreakpoint;
      self.removeBreakpoint = bkp.removeBreakpoint;
      self.getBreakpoint = bkp.getBreakpoint;
    }, true);

    this._frame.setAttribute("src", DBG_XUL);
    this._globalUI.refreshCommand();
  },

  /**
   * Closes the debugger, removing child nodes and event listeners.
   */
  close: function DP_close() {
    if (!this._tab) {
      return;
    }
    delete this._tab._scriptDebugger;
    this._tab = null;

    DebuggerPreferences.height = this._frame.height;
    this._frame.removeEventListener("Debugger:Close", this.close, true);
    this._frame.removeEventListener("unload", this.close, true);

    this._nbox.removeChild(this._splitter);
    this._nbox.removeChild(this._frame);

    this._splitter = null;
    this._frame = null;
    this._nbox = null;

    this._globalUI.refreshCommand();
  },

  /**
   * Gets the debugger content window.
   * @return nsIDOMWindow if a debugger window exists, null otherwise
   */
  get contentWindow() {
    return this._frame ? this._frame.contentWindow : null;
  },

  /**
   * Shortcut for accessing the list of breakpoints in the debugger.
   * @return object if a debugger window exists, null otherwise
   */
  get breakpoints() {
    let contentWindow = this.contentWindow;
    if (contentWindow) {
      return contentWindow.DebuggerController.Breakpoints.store;
    }
    return null;
  }
};

/**
 * Creates a window that will host a remote debugger.
 *
 * @param DebuggerUI aDebuggerUI
 *        The parent instance creating the new debugger.
 */
function RemoteDebuggerWindow(aDebuggerUI) {
  this._globalUI = aDebuggerUI;
  this._win = aDebuggerUI.chromeWindow;

  this._create();
}

RemoteDebuggerWindow.prototype = {

  /**
   * Creates and initializes the widgets containing the remote debugger UI.
   */
  _create: function DP__create() {
    this._win._remoteDebugger = this;

    this._dbgwin = this._globalUI.chromeWindow.open(DBG_XUL,
      L10N.getStr("remoteDebuggerWindowTitle"),
      "width=" + DebuggerPreferences.remoteWinWidth + "," +
      "height=" + DebuggerPreferences.remoteWinHeight + "," +
      "chrome,dependent,resizable,centerscreen");

    this._dbgwin._remoteFlag = true;

    this.close = this.close.bind(this);
    let self = this;

    this._dbgwin.addEventListener("Debugger:Loaded", function dbgLoaded() {
      self._dbgwin.removeEventListener("Debugger:Loaded", dbgLoaded, true);
      self._dbgwin.addEventListener("Debugger:Close", self.close, true);
      self._dbgwin.addEventListener("unload", self.close, true);

      // Bind shortcuts for accessing the breakpoint methods in the debugger.
      let bkp = self.contentWindow.DebuggerController.Breakpoints;
      self.addBreakpoint = bkp.addBreakpoint;
      self.removeBreakpoint = bkp.removeBreakpoint;
      self.getBreakpoint = bkp.getBreakpoint;
    }, true);
  },

  /**
   * Closes the remote debugger, along with the parent window if necessary.
   */
  close: function DP_close() {
    if (!this._win) {
      return;
    }
    delete this._win._remoteDebugger;
    this._win = null;

    this._dbgwin.close();
    this._dbgwin = null;
  },

  /**
   * Gets the remote debugger content window.
   * @return nsIDOMWindow if a debugger window exists, null otherwise.
   */
  get contentWindow() {
    return this._dbgwin;
  },

  /**
   * Shortcut for accessing the list of breakpoints in the remote debugger.
   * @return object if a debugger window exists, null otherwise.
   */
  get breakpoints() {
    let contentWindow = this.contentWindow;
    if (contentWindow) {
      return contentWindow.DebuggerController.Breakpoints.store;
    }
    return null;
  }
};

/**
 * Creates a process that will hold a chrome debugger.
 *
 * @param function aOnClose
 *        Optional, a function called when the process exits.
 * @param function aOnRun
 *        Optional, a function called when the process starts running.
 * @param nsIDOMWindow aWindow
 *        The chrome window for which the debugger instance is created.
 */
function ChromeDebuggerProcess(aWindow, aOnClose, aOnRun) {
  this._win = aWindow;
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
  _initServer: function RDP__initServer() {
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }
    DebuggerServer.closeListener();
    DebuggerServer.openListener(DebuggerPreferences.remotePort, false);
  },

  /**
   * Initializes a profile for the remote debugger process.
   */
  _initProfile: function RDP__initProfile() {
    let profileService = Cc["@mozilla.org/toolkit/profile-service;1"]
      .createInstance(Ci.nsIToolkitProfileService);

    let dbgProfileName;
    try {
      dbgProfileName = profileService.selectedProfile.name + REMOTE_PROFILE_NAME;
    } catch(e) {
      dbgProfileName = REMOTE_PROFILE_NAME;
      Cu.reportError(e);
    }

    this._dbgProfile = profileService.createProfile(null, null, dbgProfileName);
    profileService.flush();
  },

  /**
   * Creates and initializes the profile & process for the remote debugger.
   */
  _create: function RDP__create() {
    this._win._chromeDebugger = this;

    let file = FileUtils.getFile("CurProcD",
      [Services.appinfo.OS == "WINNT" ? "firefox.exe"
                                      : "firefox-bin"]);

    let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
    process.init(file);

    let args = [
      "-no-remote", "-P", this._dbgProfile.name,
      "-chrome", DBG_XUL,
      "-width", DebuggerPreferences.remoteWinWidth,
      "-height", DebuggerPreferences.remoteWinHeight];

    process.runwAsync(args, args.length, { observe: this.close.bind(this) });
    this._dbgProcess = process;

    if (typeof this._runCallback === "function") {
      this._runCallback.call({}, this);
    }
  },

  /**
   * Closes the remote debugger, removing the profile and killing the process.
   */
  close: function RDP_close() {
    if (!this._win) {
      return;
    }
    delete this._win._chromeDebugger;
    this._win = null;

    if (this._dbgProcess.isRunning) {
      this._dbgProcess.kill();
    }
    if (this._dbgProfile) {
      this._dbgProfile.remove(false);
    }
    if (typeof this._closeCallback === "function") {
      this._closeCallback.call({}, this);
    }

    this._dbgProcess = null;
    this._dbgProfile = null;
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
  getStr: function L10N_getStr(aName) {
    return this.stringBundle.GetStringFromName(aName);
  }
};

XPCOMUtils.defineLazyGetter(L10N, "stringBundle", function() {
  return Services.strings.createBundle(DBG_STRINGS_URI);
});

/**
 * Various debugger preferences.
 */
let DebuggerPreferences = {

  /**
   * Gets the preferred height of the debugger pane.
   * @return number
   */
  get height() {
    if (this._height === undefined) {
      this._height = Services.prefs.getIntPref("devtools.debugger.ui.height");
    }
    return this._height;
  },

  /**
   * Sets the preferred height of the debugger pane.
   * @param number value
   */
  set height(value) {
    Services.prefs.setIntPref("devtools.debugger.ui.height", value);
    this._height = value;
  }
};

/**
 * Gets the preferred width of the remote debugger window.
 * @return number
 */
XPCOMUtils.defineLazyGetter(DebuggerPreferences, "remoteWinWidth", function() {
  return Services.prefs.getIntPref("devtools.debugger.ui.remote-win.width");
});

/**
 * Gets the preferred height of the remote debugger window.
 * @return number
 */
XPCOMUtils.defineLazyGetter(DebuggerPreferences, "remoteWinHeight", function() {
  return Services.prefs.getIntPref("devtools.debugger.ui.remote-win.height");
});

/**
 * Gets the preferred default remote debugging host.
 * @return string
 */
XPCOMUtils.defineLazyGetter(DebuggerPreferences, "remoteHost", function() {
  return Services.prefs.getCharPref("devtools.debugger.remote-host");
});

/**
 * Gets the preferred default remote debugging port.
 * @return number
 */
XPCOMUtils.defineLazyGetter(DebuggerPreferences, "remotePort", function() {
  return Services.prefs.getIntPref("devtools.debugger.remote-port");
});
