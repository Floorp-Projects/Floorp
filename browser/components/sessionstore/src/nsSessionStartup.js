/* 
# ***** BEGIN LICENSE BLOCK *****
# * Version: MPL 1.1/GPL 2.0/LGPL 2.1
# *
# * The contents of this file are subject to the Mozilla Public License Version
# * 1.1 (the "License"); you may not use this file except in compliance with
# * the License. You may obtain a copy of the License at
# * http://www.mozilla.org/MPL/
# *
# * Software distributed under the License is distributed on an "AS IS" basis,
# * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# * for the specific language governing rights and limitations under the
# * License.
# *
# * The Original Code is the nsSessionStore component.
# *
# * The Initial Developer of the Original Code is
# * Simon Bünzli <zeniko@gmail.com>
# * Portions created by the Initial Developer are Copyright (C) 2006
# * the Initial Developer. All Rights Reserved.
# *
# * Contributor(s):
# *   Dietrich Ayala <autonome@gmail.com>
# *
# * Alternatively, the contents of this file may be used under the terms of
# * either the GNU General Public License Version 2 or later (the "GPL"), or
# * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# * in which case the provisions of the GPL or the LGPL are applicable instead
# * of those above. If you wish to allow use of your version of this file only
# * under the terms of either the GPL or the LGPL, and not to allow others to
# * use your version of this file under the terms of the MPL, indicate your
# * decision by deleting the provisions above and replace them with the notice
# * and other provisions required by the GPL or the LGPL. If you do not delete
# * the provisions above, a recipient may use your version of this file under
# * the terms of any one of the MPL, the GPL or the LGPL.
# *
# * ***** END LICENSE BLOCK ***** 
*/

/**
# * Session Storage and Restoration
# * 
# * Overview
# * This service reads user's session file at startup, and makes a determination 
# * as to whether the session should be restored. It will restore the session 
# * under the circumstances described below.
# * 
# * Crash Detection
# * The session file stores a session.state property, that 
# * indicates whether the browser is currently running. When the browser shuts 
# * down, the field is changed to "stopped". At startup, this field is read, and
# * if its value is "running", then it's assumed that the browser had previously
# * crashed, or at the very least that something bad happened, and that we should
# * restore the session.
# * 
# * Forced Restarts
# * In the event that a restart is required due to application update or extension
# * installation, set the browser.sessionstore.resume_session_once pref to true,
# * and the session will be restored the next time the browser starts.
# * 
# * Always Resume
# * This service will always resume the session if the integer pref 
# * browser.startup.page is set to 3.
*/

/* :::::::: Constants and Helpers ::::::::::::::: */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const STATE_RUNNING_STR = "running";

function debug(aMsg) {
  aMsg = ("SessionStartup: " + aMsg).replace(/\S{80}/g, "$&\n");
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService)
                                     .logStringMessage(aMsg);
}

/* :::::::: The Service ::::::::::::::: */

function SessionStartup() {
}

SessionStartup.prototype = {

  // the state to restore at startup
  _iniString: null,
  _sessionType: Ci.nsISessionStartup.NO_SESSION,

/* ........ Global Event Handlers .............. */

  /**
   * Initialize the component
   */
  init: function sss_init() {
    let prefBranch = Cc["@mozilla.org/preferences-service;1"].
                     getService(Ci.nsIPrefService).getBranch("browser.");

    // get file references
    var dirService = Cc["@mozilla.org/file/directory_service;1"].
                     getService(Ci.nsIProperties);
    let sessionFile = dirService.get("ProfD", Ci.nsILocalFile);
    sessionFile.append("sessionstore.js");
    
    let doResumeSession = prefBranch.getBoolPref("sessionstore.resume_session_once") ||
                          prefBranch.getIntPref("startup.page") == 3;
    
    // only read the session file if config allows possibility of restoring
    var resumeFromCrash = prefBranch.getBoolPref("sessionstore.resume_from_crash");
    if (!resumeFromCrash && !doResumeSession || !sessionFile.exists())
      return;
    
    // get string containing session state
    this._iniString = this._readStateFile(sessionFile);
    if (!this._iniString)
      return;
    
    try {
      // parse the session state into JS objects
      var s = new Components.utils.Sandbox("about:blank");
      var initialState = Components.utils.evalInSandbox(this._iniString, s);
    }
    catch (ex) { debug("The session file is invalid: " + ex); } 
    
    let lastSessionCrashed =
      initialState && initialState.session && initialState.session.state &&
      initialState.session.state == STATE_RUNNING_STR;
    
    // set the startup type
    if (lastSessionCrashed && resumeFromCrash)
      this._sessionType = Ci.nsISessionStartup.RECOVER_SESSION;
    else if (!lastSessionCrashed && doResumeSession)
      this._sessionType = Ci.nsISessionStartup.RESUME_SESSION;
    else
      this._iniString = null; // reset the state string

    if (this._sessionType != Ci.nsISessionStartup.NO_SESSION) {
      // wait for the first browser window to open
      var observerService = Cc["@mozilla.org/observer-service;1"].
                            getService(Ci.nsIObserverService);
      observerService.addObserver(this, "domwindowopened", true);
      observerService.addObserver(this, "browser:purge-session-history", true);
    }
  },

  /**
   * Handle notifications
   */
  observe: function sss_observe(aSubject, aTopic, aData) {
    var observerService = Cc["@mozilla.org/observer-service;1"].
                          getService(Ci.nsIObserverService);

    switch (aTopic) {
    case "app-startup": 
      observerService.addObserver(this, "final-ui-startup", true);
      observerService.addObserver(this, "quit-application", true);
      break;
    case "final-ui-startup": 
      observerService.removeObserver(this, "final-ui-startup");
      observerService.removeObserver(this, "quit-application");
      this.init();
      break;
    case "quit-application":
      // no reason for initializing at this point (cf. bug 409115)
      observerService.removeObserver(this, "final-ui-startup");
      observerService.removeObserver(this, "quit-application");
      break;
    case "domwindowopened":
      var window = aSubject;
      var self = this;
      window.addEventListener("load", function() {
        self._onWindowOpened(window);
        window.removeEventListener("load", arguments.callee, false);
      }, false);
      break;
    case "browser:purge-session-history":
      // reset all state on sanitization
      this._iniString = null;
      this._sessionType = Ci.nsISessionStartup.NO_SESSION;
      // no need in repeating this, since startup state won't change
      observerService.removeObserver(this, "browser:purge-session-history");
      break;
    }
  },

  /**
   * Removes the default arguments from the first browser window
   * (and removes the "domwindowopened" observer afterwards).
   */
  _onWindowOpened: function sss_onWindowOpened(aWindow) {
    var wType = aWindow.document.documentElement.getAttribute("windowtype");
    if (wType != "navigator:browser")
      return;
    
    /**
     * Note: this relies on the fact that nsBrowserContentHandler will return
     * a different value the first time its getter is called after an update,
     * due to its needHomePageOverride() logic. We don't want to remove the
     * default arguments in the update case, since they include the "What's
     * New" page.
     *
     * Since we're garanteed to be at least the second caller of defaultArgs
     * (nsBrowserContentHandler calls it to determine which arguments to pass
     * at startup), we know that if the window's arguments don't match the
     * current defaultArguments, we're either in the update case, or we're
     * launching a non-default browser window, so we shouldn't remove the
     * window's arguments.
     */
    var defaultArgs = Cc["@mozilla.org/browser/clh;1"].
                      getService(Ci.nsIBrowserHandler).defaultArgs;
    if (aWindow.arguments && aWindow.arguments[0] &&
        aWindow.arguments[0] == defaultArgs)
      aWindow.arguments[0] = null;
    
    var observerService = Cc["@mozilla.org/observer-service;1"].
                          getService(Ci.nsIObserverService);
    observerService.removeObserver(this, "domwindowopened");
  },

/* ........ Public API ................*/

  /**
   * Get the session state as a string
   */
  get state() {
    return this._iniString;
  },

  /**
   * Determine whether there is a pending session restore.
   * @returns bool
   */
  doRestore: function sss_doRestore() {
    return this._sessionType != Ci.nsISessionStartup.NO_SESSION;
  },

  /**
   * Get the type of pending session store, if any.
   */
  get sessionType() {
    return this._sessionType;
  },

/* ........ Storage API .............. */

  /**
   * Reads a session state file into a string and lets
   * observers modify the state before it's being used
   *
   * @param aFile is any nsIFile
   * @returns a session state string
   */
  _readStateFile: function sss_readStateFile(aFile) {
    var stateString = Cc["@mozilla.org/supports-string;1"].
                        createInstance(Ci.nsISupportsString);
    stateString.data = this._readFile(aFile) || "";
    
    var observerService = Cc["@mozilla.org/observer-service;1"].
                          getService(Ci.nsIObserverService);
    observerService.notifyObservers(stateString, "sessionstore-state-read", "");
    
    return stateString.data;
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
    catch (ex) { Components.utils.reportError(ex); }
    
    return null;
  },

  /* ........ QueryInterface .............. */
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference,
                                          Ci.nsISessionStartup]),
  classDescription: "Browser Session Startup Service",
  classID:          Components.ID("{ec7a6c20-e081-11da-8ad9-0800200c9a66}"),
  contractID:       "@mozilla.org/browser/sessionstartup;1",

  // get this contractID registered for certain categories via XPCOMUtils
  _xpcom_categories: [
    // make ourselves a startup observer
    { category: "app-startup", service: true }
  ]

};

function NSGetModule(aCompMgr, aFileSpec)
  XPCOMUtils.generateModule([SessionStartup]);
