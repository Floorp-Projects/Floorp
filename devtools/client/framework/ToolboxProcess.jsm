/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { interfaces: Ci, utils: Cu, results: Cr } = Components;

const DBG_XUL = "chrome://devtools/content/framework/toolbox-process-window.xul";
const CHROME_DEBUGGER_PROFILE_NAME = "chrome_debugger_profile";

const { require, DevToolsLoader } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Subprocess", "resource://gre/modules/Subprocess.jsm");
XPCOMUtils.defineLazyGetter(this, "Telemetry", function () {
  return require("devtools/client/shared/telemetry");
});
XPCOMUtils.defineLazyGetter(this, "EventEmitter", function () {
  return require("devtools/shared/event-emitter");
});
const promise = require("promise");
const Services = require("Services");

this.EXPORTED_SYMBOLS = ["BrowserToolboxProcess"];

var processes = new Set();

/**
 * Constructor for creating a process that will hold a chrome toolbox.
 *
 * @param function onClose [optional]
 *        A function called when the process stops running.
 * @param function onRun [optional]
 *        A function called when the process starts running.
 * @param object options [optional]
 *        An object with properties for configuring BrowserToolboxProcess.
 */
this.BrowserToolboxProcess = function BrowserToolboxProcess(onClose, onRun, options) {
  let emitter = new EventEmitter();
  this.on = emitter.on.bind(emitter);
  this.off = emitter.off.bind(emitter);
  this.once = emitter.once.bind(emitter);
  // Forward any events to the shared emitter.
  this.emit = function (...args) {
    emitter.emit(...args);
    BrowserToolboxProcess.emit(...args);
  };

  // If first argument is an object, use those properties instead of
  // all three arguments
  if (typeof onClose === "object") {
    if (onClose.onClose) {
      this.once("close", onClose.onClose);
    }
    if (onClose.onRun) {
      this.once("run", onClose.onRun);
    }
    this._options = onClose;
  } else {
    if (onClose) {
      this.once("close", onClose);
    }
    if (onRun) {
      this.once("run", onRun);
    }
    this._options = options || {};
  }

  this._telemetry = new Telemetry();

  this.close = this.close.bind(this);
  Services.obs.addObserver(this.close, "quit-application");
  this._initServer();
  this._initProfile();
  this._create();

  processes.add(this);
};

EventEmitter.decorate(BrowserToolboxProcess);

/**
 * Initializes and starts a chrome toolbox process.
 * @return object
 */
BrowserToolboxProcess.init = function (onClose, onRun, options) {
  return new BrowserToolboxProcess(onClose, onRun, options);
};

/**
 * Passes a set of options to the BrowserAddonActors for the given ID.
 *
 * @param id string
 *        The ID of the add-on to pass the options to
 * @param options object
 *        The options.
 * @return a promise that will be resolved when complete.
 */
BrowserToolboxProcess.setAddonOptions = function (id, options) {
  let promises = [];

  for (let process of processes.values()) {
    promises.push(process.debuggerServer.setAddonOptions(id, options));
  }

  return promise.all(promises);
};

BrowserToolboxProcess.prototype = {
  /**
   * Initializes the debugger server.
   */
  _initServer: function () {
    if (this.debuggerServer) {
      dumpn("The chrome toolbox server is already running.");
      return;
    }

    dumpn("Initializing the chrome toolbox server.");

    // Create a separate loader instance, so that we can be sure to receive a
    // separate instance of the DebuggingServer from the rest of the devtools.
    // This allows us to safely use the tools against even the actors and
    // DebuggingServer itself, especially since we can mark this loader as
    // invisible to the debugger (unlike the usual loader settings).
    this.loader = new DevToolsLoader();
    this.loader.invisibleToDebugger = true;
    let { DebuggerServer } = this.loader.require("devtools/server/main");
    this.debuggerServer = DebuggerServer;
    dumpn("Created a separate loader instance for the DebuggerServer.");

    // Forward interesting events.
    this.debuggerServer.on("connectionchange", this.emit);

    this.debuggerServer.init();
    // We mainly need a root actor and tab actors for opening a toolbox, even
    // against chrome/content/addon. But the "no auto hide" button uses the
    // preference actor, so also register the browser actors.
    this.debuggerServer.registerActors({ root: true, browser: true, tab: true });
    this.debuggerServer.allowChromeProcess = true;
    dumpn("initialized and added the browser actors for the DebuggerServer.");

    let chromeDebuggingPort =
      Services.prefs.getIntPref("devtools.debugger.chrome-debugging-port");
    let chromeDebuggingWebSocket =
      Services.prefs.getBoolPref("devtools.debugger.chrome-debugging-websocket");
    let listener = this.debuggerServer.createListener();
    listener.portOrPath = chromeDebuggingPort;
    listener.webSocket = chromeDebuggingWebSocket;
    listener.open();

    dumpn("Finished initializing the chrome toolbox server.");
    dumpn("Started listening on port: " + chromeDebuggingPort);
  },

  /**
   * Initializes a profile for the remote debugger process.
   */
  _initProfile: function () {
    dumpn("Initializing the chrome toolbox user profile.");

    // We used to use `ProfLD` instead of `ProfD`, so migrate old profiles if they exist.
    this._migrateProfileDir();

    let debuggingProfileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
    debuggingProfileDir.append(CHROME_DEBUGGER_PROFILE_NAME);
    try {
      debuggingProfileDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    } catch (ex) {
      // Don't re-copy over the prefs again if this profile already exists
      if (ex.result === Cr.NS_ERROR_FILE_ALREADY_EXISTS) {
        this._dbgProfilePath = debuggingProfileDir.path;
      } else {
        dumpn("Error trying to create a profile directory, failing.");
        dumpn("Error: " + (ex.message || ex));
      }
      return;
    }

    this._dbgProfilePath = debuggingProfileDir.path;

    // We would like to copy prefs into this new profile...
    let prefsFile = debuggingProfileDir.clone();
    prefsFile.append("prefs.js");
    // ... but unfortunately, when we run tests, it seems the starting profile
    // clears out the prefs file before re-writing it, and in practice the
    // file is empty when we get here. So just copying doesn't work in that
    // case.
    // We could force a sync pref flush and then copy it... but if we're doing
    // that, we might as well just flush directly to the new profile, which
    // always works:
    Services.prefs.savePrefFile(prefsFile);

    dumpn("Finished creating the chrome toolbox user profile at: " +
          this._dbgProfilePath);
  },

  /**
   * Originally, the profile was placed in `ProfLD` instead of `ProfD`.  On some systems,
   * such as macOS, `ProfLD` is in the user's Caches directory, which is not an
   * appropriate place to store supposedly persistent profile data.
   */
  _migrateProfileDir() {
    let oldDebuggingProfileDir = Services.dirsvc.get("ProfLD", Ci.nsIFile);
    oldDebuggingProfileDir.append(CHROME_DEBUGGER_PROFILE_NAME);
    if (!oldDebuggingProfileDir.exists()) {
      return;
    }
    dumpn(`Old debugging profile exists: ${oldDebuggingProfileDir.path}`);
    try {
      // Remove the directory from the target location, if it exists
      let newDebuggingProfileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
      newDebuggingProfileDir.append(CHROME_DEBUGGER_PROFILE_NAME);
      if (newDebuggingProfileDir.exists()) {
        dumpn(`Removing folder at destination: ${newDebuggingProfileDir.path}`);
        newDebuggingProfileDir.remove(true);
      }
      // Move profile from old to new location
      let newDebuggingProfileParent = Services.dirsvc.get("ProfD", Ci.nsIFile);
      oldDebuggingProfileDir.moveTo(newDebuggingProfileParent, null);
      dumpn("Debugging profile migrated successfully");
    } catch (e) {
      dumpn(`Debugging profile migration failed: ${e}`);
    }
  },

  /**
   * Creates and initializes the profile & process for the remote debugger.
   */
  _create: function () {
    dumpn("Initializing chrome debugging process.");

    let command = Services.dirsvc.get("XREExeF", Ci.nsIFile).path;

    let xulURI = DBG_XUL;

    if (this._options.addonID) {
      xulURI += "?addonID=" + this._options.addonID;
    }

    dumpn("Running chrome debugging process.");
    let args = [
      "-no-remote",
      "-foreground",
      "-profile", this._dbgProfilePath,
      "-chrome", xulURI
    ];

    // During local development, incremental builds can trigger the main process
    // to clear its startup cache with the "flag file" .purgecaches, but this
    // file is removed during app startup time, so we aren't able to know if it
    // was present in order to also clear the child profile's startup cache as
    // well.
    //
    // As an approximation of "isLocalBuild", check for an unofficial build.
    if (!Services.appinfo.isOfficial) {
      args.push("-purgecaches");
    }

    this._dbgProcessPromise = Subprocess.call({
      command,
      arguments: args,
      environmentAppend: true,
      environment: {
        // Disable safe mode for the new process in case this was opened via the
        // keyboard shortcut.
        MOZ_DISABLE_SAFE_MODE_KEY: "1",
      },
    }).then(proc => {
      this._dbgProcess = proc;

      this._telemetry.toolOpened("jsbrowserdebugger");

      dumpn("Chrome toolbox is now running...");
      this.emit("run", this);

      proc.stdin.close();
      proc.wait().then(() => this.close());
      return proc;
    });
  },

  /**
   * Closes the remote debugging server and kills the toolbox process.
   */
  close: async function () {
    if (this.closed) {
      return;
    }

    dumpn("Cleaning up the chrome debugging process.");
    Services.obs.removeObserver(this.close, "quit-application");

    this._dbgProcess.stdout.close();
    await this._dbgProcess.kill();

    this._telemetry.toolClosed("jsbrowserdebugger");
    if (this.debuggerServer) {
      this.debuggerServer.off("connectionchange", this.emit);
      this.debuggerServer.destroy();
      this.debuggerServer = null;
    }

    dumpn("Chrome toolbox is now closed...");
    this.closed = true;
    this.emit("close", this);
    processes.delete(this);

    this._dbgProcess = null;
    this._options = null;
    if (this.loader) {
      this.loader.destroy();
    }
    this.loader = null;
    this._telemetry = null;
  }
};

/**
 * Helper method for debugging.
 * @param string
 */
function dumpn(str) {
  if (wantLogging) {
    dump("DBG-FRONTEND: " + str + "\n");
  }
}

var wantLogging = Services.prefs.getBoolPref("devtools.debugger.log");

Services.prefs.addObserver("devtools.debugger.log", {
  observe: (...args) => {
    wantLogging = Services.prefs.getBoolPref(args.pop());
  }
});

Services.obs.notifyObservers(null, "ToolboxProcessLoaded");
