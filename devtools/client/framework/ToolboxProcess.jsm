/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DBG_XUL =
  "chrome://devtools/content/framework/toolbox-process-window.html";
const CHROME_DEBUGGER_PROFILE_NAME = "chrome_debugger_profile";

const { require, DevToolsLoader } = ChromeUtils.import(
  "resource://devtools/shared/Loader.jsm"
);
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "Subprocess",
  "resource://gre/modules/Subprocess.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);

XPCOMUtils.defineLazyGetter(this, "Telemetry", function() {
  return require("devtools/client/shared/telemetry");
});
XPCOMUtils.defineLazyGetter(this, "EventEmitter", function() {
  return require("devtools/shared/event-emitter");
});

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
 */
this.BrowserToolboxProcess = function BrowserToolboxProcess(onClose, onRun) {
  const emitter = new EventEmitter();
  this.on = emitter.on.bind(emitter);
  this.off = emitter.off.bind(emitter);
  this.once = emitter.once.bind(emitter);
  // Forward any events to the shared emitter.
  this.emit = function(...args) {
    emitter.emit(...args);
    BrowserToolboxProcess.emit(...args);
  };

  if (onClose) {
    this.once("close", onClose);
  }
  if (onRun) {
    this.once("run", onRun);
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
BrowserToolboxProcess.init = function(onClose, onRun) {
  if (
    !Services.prefs.getBoolPref("devtools.chrome.enabled") ||
    !Services.prefs.getBoolPref("devtools.debugger.remote-enabled")
  ) {
    console.error("Could not start Browser Toolbox, you need to enable it.");
    return null;
  }
  return new BrowserToolboxProcess(onClose, onRun);
};

/**
 * Figure out if there are any open Browser Toolboxes that'll need to be restored.
 * @return bool
 */
BrowserToolboxProcess.getBrowserToolboxSessionState = function() {
  return processes.size !== 0;
};

BrowserToolboxProcess.prototype = {
  /**
   * Initializes the debugger server.
   */
  _initServer: function() {
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
    this.loader = new DevToolsLoader({
      invisibleToDebugger: true,
    });
    const { DebuggerServer } = this.loader.require(
      "devtools/server/debugger-server"
    );
    const { SocketListener } = this.loader.require(
      "devtools/shared/security/socket"
    );
    this.debuggerServer = DebuggerServer;
    dumpn("Created a separate loader instance for the DebuggerServer.");

    this.debuggerServer.init();
    // We mainly need a root actor and target actors for opening a toolbox, even
    // against chrome/content. But the "no auto hide" button uses the
    // preference actor, so also register the browser actors.
    this.debuggerServer.registerAllActors();
    this.debuggerServer.allowChromeProcess = true;
    dumpn("initialized and added the browser actors for the DebuggerServer.");

    const chromeDebuggingWebSocket = Services.prefs.getBoolPref(
      "devtools.debugger.chrome-debugging-websocket"
    );
    const socketOptions = {
      portOrPath: -1,
      webSocket: chromeDebuggingWebSocket,
    };
    const listener = new SocketListener(this.debuggerServer, socketOptions);
    listener.open();
    this.listener = listener;
    this.port = listener.port;

    if (!this.port) {
      throw new Error("No debugger server port");
    }

    dumpn("Finished initializing the chrome toolbox server.");
    dump(
      `Debugger Server for Browser Toolbox listening on port: ${this.port}\n`
    );
  },

  /**
   * Initializes a profile for the remote debugger process.
   */
  _initProfile: function() {
    dumpn("Initializing the chrome toolbox user profile.");

    // We used to use `ProfLD` instead of `ProfD`, so migrate old profiles if they exist.
    this._migrateProfileDir();

    const debuggingProfileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
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
    const prefsFile = debuggingProfileDir.clone();
    prefsFile.append("prefs.js");
    // ... but unfortunately, when we run tests, it seems the starting profile
    // clears out the prefs file before re-writing it, and in practice the
    // file is empty when we get here. So just copying doesn't work in that
    // case.
    // We could force a sync pref flush and then copy it... but if we're doing
    // that, we might as well just flush directly to the new profile, which
    // always works:
    Services.prefs.savePrefFile(prefsFile);

    dumpn(
      "Finished creating the chrome toolbox user profile at: " +
        this._dbgProfilePath
    );
  },

  /**
   * Originally, the profile was placed in `ProfLD` instead of `ProfD`.  On some systems,
   * such as macOS, `ProfLD` is in the user's Caches directory, which is not an
   * appropriate place to store supposedly persistent profile data.
   */
  _migrateProfileDir() {
    const oldDebuggingProfileDir = Services.dirsvc.get("ProfLD", Ci.nsIFile);
    const newDebuggingProfileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
    if (oldDebuggingProfileDir.path == newDebuggingProfileDir.path) {
      // It's possible for these locations to be the same, such as running from
      // a custom profile directory specified via CLI.
      return;
    }
    oldDebuggingProfileDir.append(CHROME_DEBUGGER_PROFILE_NAME);
    if (!oldDebuggingProfileDir.exists()) {
      return;
    }
    dumpn(`Old debugging profile exists: ${oldDebuggingProfileDir.path}`);
    try {
      // Remove the directory from the target location, if it exists
      newDebuggingProfileDir.append(CHROME_DEBUGGER_PROFILE_NAME);
      if (newDebuggingProfileDir.exists()) {
        dumpn(`Removing folder at destination: ${newDebuggingProfileDir.path}`);
        newDebuggingProfileDir.remove(true);
      }
      // Move profile from old to new location
      const newDebuggingProfileParent = Services.dirsvc.get(
        "ProfD",
        Ci.nsIFile
      );
      oldDebuggingProfileDir.moveTo(newDebuggingProfileParent, null);
      dumpn("Debugging profile migrated successfully");
    } catch (e) {
      dumpn(`Debugging profile migration failed: ${e}`);
    }
  },

  /**
   * Creates and initializes the profile & process for the remote debugger.
   */
  _create: function() {
    dumpn("Initializing chrome debugging process.");

    const command = Services.dirsvc.get("XREExeF", Ci.nsIFile).path;

    dumpn("Running chrome debugging process.");
    const args = [
      "-no-remote",
      "-foreground",
      "-profile",
      this._dbgProfilePath,
      "-chrome",
      DBG_XUL,
    ];
    const environment = {
      // Disable safe mode for the new process in case this was opened via the
      // keyboard shortcut.
      MOZ_DISABLE_SAFE_MODE_KEY: "1",
      MOZ_BROWSER_TOOLBOX_PORT: String(this.port),
    };

    // During local development, incremental builds can trigger the main process
    // to clear its startup cache with the "flag file" .purgecaches, but this
    // file is removed during app startup time, so we aren't able to know if it
    // was present in order to also clear the child profile's startup cache as
    // well.
    //
    // As an approximation of "isLocalBuild", check for an unofficial build.
    if (!AppConstants.MOZILLA_OFFICIAL) {
      args.push("-purgecaches");
    }

    this._dbgProcessPromise = Subprocess.call({
      command,
      arguments: args,
      environmentAppend: true,
      stderr: "stdout",
      environment,
    }).then(
      proc => {
        this._dbgProcess = proc;

        // jsbrowserdebugger is not connected with a toolbox so we pass -1 as the
        // toolbox session id.
        this._telemetry.toolOpened("jsbrowserdebugger", -1, this);

        dumpn("Chrome toolbox is now running...");
        this.emit("run", this);

        proc.stdin.close();
        const dumpPipe = async pipe => {
          let data = await pipe.readString();
          while (data) {
            dump(data);
            data = await pipe.readString();
          }
        };
        dumpPipe(proc.stdout);

        proc.wait().then(() => this.close());

        return proc;
      },
      err => {
        console.log(
          `Error loading Browser Toolbox: ${command} ${args.join(" ")}`,
          err
        );
      }
    );
  },

  /**
   * Closes the remote debugging server and kills the toolbox process.
   */
  close: async function() {
    if (this.closed) {
      return;
    }

    this.closed = true;

    dumpn("Cleaning up the chrome debugging process.");

    Services.obs.removeObserver(this.close, "quit-application");

    this._dbgProcess.stdout.close();
    await this._dbgProcess.kill();

    // jsbrowserdebugger is not connected with a toolbox so we pass -1 as the
    // toolbox session id.
    this._telemetry.toolClosed("jsbrowserdebugger", -1, this);

    if (this.listener) {
      this.listener.close();
    }

    if (this.debuggerServer) {
      this.debuggerServer.destroy();
      this.debuggerServer = null;
    }

    dumpn("Chrome toolbox is now closed...");
    this.emit("close", this);
    processes.delete(this);

    this._dbgProcess = null;
    if (this.loader) {
      this.loader.destroy();
    }
    this.loader = null;
    this._telemetry = null;
  },
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
  },
});
