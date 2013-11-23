/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const DBG_XUL = "chrome://browser/content/devtools/debugger.xul";
const CHROME_DEBUGGER_PROFILE_NAME = "-chrome-debugger";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");

Cu.import("resource://gre/modules/devtools/Loader.jsm");
let require = devtools.require;
let Telemetry = require("devtools/shared/telemetry");

this.EXPORTED_SYMBOLS = ["BrowserDebuggerProcess"];

/**
 * Constructor for creating a process that will hold a chrome debugger.
 *
 * @param function aOnClose [optional]
 *        A function called when the process stops running.
 * @param function aOnRun [optional]
 *        A function called when the process starts running.
 */
this.BrowserDebuggerProcess = function BrowserDebuggerProcess(aOnClose, aOnRun) {
  this._closeCallback = aOnClose;
  this._runCallback = aOnRun;
  this._telemetry = new Telemetry();

  this._initServer();
  this._initProfile();
  this._create();
};

/**
 * Initializes and starts a chrome debugger process.
 * @return object
 */
BrowserDebuggerProcess.init = function(aOnClose, aOnRun) {
  return new BrowserDebuggerProcess(aOnClose, aOnRun);
};

BrowserDebuggerProcess.prototype = {
  /**
   * Initializes the debugger server.
   */
  _initServer: function() {
    dumpn("Initializing the chrome debugger server.");

    if (!this.loader) {
      // Create a separate loader instance, so that we can be sure to receive a
      // separate instance of the DebuggingServer from the rest of the devtools.
      // This allows us to safely use the tools against even the actors and
      // DebuggingServer itself.
      this.loader = new DevToolsLoader();
      this.loader.main("devtools/server/main");
      this.debuggerServer = this.loader.DebuggerServer;
      dumpn("Created a separate loader instance for the DebuggerServer.");
    }

    if (!this.debuggerServer.initialized) {
      this.debuggerServer.init();
      this.debuggerServer.addBrowserActors();
      dumpn("initialized and added the browser actors for the DebuggerServer.");
    }

    this.debuggerServer.openListener(Prefs.chromeDebuggingPort);

    dumpn("Finished initializing the chrome debugger server.");
    dumpn("Started listening on port: " + Prefs.chromeDebuggingPort);
  },

  /**
   * Initializes a profile for the remote debugger process.
   */
  _initProfile: function() {
    dumpn("Initializing the chrome debugger user profile.");

    let profileService = Cc["@mozilla.org/toolkit/profile-service;1"]
      .createInstance(Ci.nsIToolkitProfileService);

    let profileName;
    try {
      // Attempt to get the required chrome debugging profile name string.
      profileName = profileService.selectedProfile.name + CHROME_DEBUGGER_PROFILE_NAME;
      dumpn("Using chrome debugger profile name: " + profileName);
    } catch (e) {
      // Requested profile string could not be retrieved.
      profileName = CHROME_DEBUGGER_PROFILE_NAME;
      let msg = "Querying the current profile failed. " + e.name + ": " + e.message;
      dumpn(msg);
      Cu.reportError(msg);
    }

    let profileObject;
    try {
      // Attempt to get the required chrome debugging profile toolkit object.
      profileObject = profileService.getProfileByName(profileName);
      dumpn("Using chrome debugger profile object: " + profileObject);

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
      dumpn("The already existing chrome debugger profile was invalid.");
    } catch (e) {
      // Requested profile object was not found.
      let msg = "Creating a profile failed. " + e.name + ": " + e.message;
      dumpn(msg);
      Cu.reportError(msg);
    }

    // Create a new chrome debugging profile.
    this._dbgProfile = profileService.createProfile(null, profileName);
    profileService.flush();

    dumpn("Finished creating the chrome debugger user profile.");
    dumpn("Flushed profile service with: " + profileName);
  },

  /**
   * Creates and initializes the profile & process for the remote debugger.
   */
  _create: function() {
    dumpn("Initializing chrome debugging process.");
    let process = this._dbgProcess = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
    process.init(Services.dirsvc.get("XREExeF", Ci.nsIFile));

    dumpn("Running chrome debugging process.");
    let args = ["-no-remote", "-foreground", "-P", this._dbgProfile.name, "-chrome", DBG_XUL];
    process.runwAsync(args, args.length, { observe: () => this.close() });

    this._telemetry.toolOpened("jsbrowserdebugger");

    dumpn("Chrome debugger is now running...");
    if (typeof this._runCallback == "function") {
      this._runCallback.call({}, this);
    }
  },

  /**
   * Closes the remote debugger, removing the profile and killing the process.
   */
  close: function() {
    dumpn("Cleaning up the chrome debugging process.");

    if (this._dbgProcess.isRunning) {
      this._dbgProcess.kill();
    }

    this._telemetry.toolClosed("jsbrowserdebugger");
    this.debuggerServer.destroy();

    dumpn("Chrome debugger is now closed...");
    if (typeof this._closeCallback == "function") {
      this._closeCallback.call({}, this);
    }
  }
};

/**
 * Shortcuts for accessing various debugger preferences.
 */
let Prefs = new ViewHelpers.Prefs("devtools.debugger", {
  chromeDebuggingHost: ["Char", "chrome-debugging-host"],
  chromeDebuggingPort: ["Int", "chrome-debugging-port"]
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

Services.prefs.addObserver("devtools.debugger.log", {
  observe: (...args) => wantLogging = Services.prefs.getBoolPref(args.pop())
}, false);
