/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Keep this synchronized with the value of the same name in
// toolkit/xre/nsAppRunner.cpp.
const BROWSER_TOOLBOX_WINDOW_URL =
  "chrome://devtools/content/framework/browser-toolbox/window.html";
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
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);

XPCOMUtils.defineLazyGetter(this, "Telemetry", function() {
  return require("devtools/client/shared/telemetry");
});
XPCOMUtils.defineLazyGetter(this, "EventEmitter", function() {
  return require("devtools/shared/event-emitter");
});

const Services = require("Services");

const EXPORTED_SYMBOLS = ["BrowserToolboxLauncher"];

var processes = new Set();

/**
 * Constructor for creating a process that will hold a chrome toolbox.
 *
 * @param function onClose [optional]
 *        A function called when the process stops running.
 * @param function onRun [optional]
 *        A function called when the process starts running.
 * @param boolean overwritePreferences [optional]
 *        Set to force overwriting the toolbox profile's preferences with the
 *        current set of preferences.
 */
function BrowserToolboxLauncher(
  onClose,
  onRun,
  overwritePreferences,
  binaryPath
) {
  const emitter = new EventEmitter();
  this.on = emitter.on.bind(emitter);
  this.off = emitter.off.bind(emitter);
  this.once = emitter.once.bind(emitter);
  // Forward any events to the shared emitter.
  this.emit = function(...args) {
    emitter.emit(...args);
    BrowserToolboxLauncher.emit(...args);
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
  this._initProfile(overwritePreferences);
  this._create(binaryPath);

  processes.add(this);
}

EventEmitter.decorate(BrowserToolboxLauncher);

/**
 * Initializes and starts a chrome toolbox process.
 * @return object
 */
BrowserToolboxLauncher.init = function(
  onClose,
  onRun,
  overwritePreferences,
  binaryPath
) {
  if (
    !Services.prefs.getBoolPref("devtools.chrome.enabled") ||
    !Services.prefs.getBoolPref("devtools.debugger.remote-enabled")
  ) {
    console.error("Could not start Browser Toolbox, you need to enable it.");
    return null;
  }
  return new BrowserToolboxLauncher(
    onClose,
    onRun,
    overwritePreferences,
    binaryPath
  );
};

/**
 * Figure out if there are any open Browser Toolboxes that'll need to be restored.
 * @return bool
 */
BrowserToolboxLauncher.getBrowserToolboxSessionState = function() {
  return processes.size !== 0;
};

BrowserToolboxLauncher.prototype = {
  /**
   * Initializes the devtools server.
   */
  _initServer: function() {
    if (this.devToolsServer) {
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
    const { DevToolsServer } = this.loader.require(
      "devtools/server/devtools-server"
    );
    const { SocketListener } = this.loader.require(
      "devtools/shared/security/socket"
    );
    this.devToolsServer = DevToolsServer;
    dumpn("Created a separate loader instance for the DevToolsServer.");

    this.devToolsServer.init();
    // We mainly need a root actor and target actors for opening a toolbox, even
    // against chrome/content. But the "no auto hide" button uses the
    // preference actor, so also register the browser actors.
    this.devToolsServer.registerAllActors();
    this.devToolsServer.allowChromeProcess = true;
    dumpn("initialized and added the browser actors for the DevToolsServer.");

    const chromeDebuggingWebSocket = Services.prefs.getBoolPref(
      "devtools.debugger.chrome-debugging-websocket"
    );
    const socketOptions = {
      fromBrowserToolbox: true,
      portOrPath: -1,
      webSocket: chromeDebuggingWebSocket,
    };
    const listener = new SocketListener(this.devToolsServer, socketOptions);
    listener.open();
    this.listener = listener;
    this.port = listener.port;

    if (!this.port) {
      throw new Error("No devtools server port");
    }

    dumpn("Finished initializing the chrome toolbox server.");
    dump(
      `DevTools Server for Browser Toolbox listening on port: ${this.port}\n`
    );
  },

  /**
   * Initializes a profile for the remote debugger process.
   */
  _initProfile(overwritePreferences) {
    dumpn("Initializing the chrome toolbox user profile.");

    const debuggingProfileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
    debuggingProfileDir.append(CHROME_DEBUGGER_PROFILE_NAME);
    try {
      debuggingProfileDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    } catch (ex) {
      if (ex.result === Cr.NS_ERROR_FILE_ALREADY_EXISTS) {
        if (!overwritePreferences) {
          this._dbgProfilePath = debuggingProfileDir.path;
          return;
        }
        // Fall through and copy the current set of prefs to the profile.
      } else {
        dumpn("Error trying to create a profile directory, failing.");
        dumpn("Error: " + (ex.message || ex));
        return;
      }
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
   * Creates and initializes the profile & process for the remote debugger.
   */
  _create: function(binaryPath) {
    dumpn("Initializing chrome debugging process.");

    let command = Services.dirsvc.get("XREExeF", Ci.nsIFile).path;
    let profilePath = this._dbgProfilePath;

    if (binaryPath) {
      command = binaryPath;
      profilePath = FileUtils.getDir("TmpD", ["browserToolboxProfile"], true)
        .path;
    }

    dumpn("Running chrome debugging process.");
    const args = [
      "-no-remote",
      "-foreground",
      "-profile",
      profilePath,
      "-chrome",
      BROWSER_TOOLBOX_WINDOW_URL,
    ];

    const isBrowserToolboxFission = Services.prefs.getBoolPref(
      "devtools.browsertoolbox.fission",
      false
    );
    const isInputContextEnabled = Services.prefs.getBoolPref(
      "devtools.webconsole.input.context",
      false
    );
    const environment = {
      // Will be read by the Browser Toolbox Firefox instance to update the
      // devtools.browsertoolbox.fission pref on the Browser Toolbox profile.
      MOZ_BROWSER_TOOLBOX_FISSION_PREF: isBrowserToolboxFission ? "1" : "0",
      // Similar, but for the WebConsole input context dropdown.
      MOZ_BROWSER_TOOLBOX_INPUT_CONTEXT: isInputContextEnabled ? "1" : "0",
      // Disable safe mode for the new process in case this was opened via the
      // keyboard shortcut.
      MOZ_DISABLE_SAFE_MODE_KEY: "1",
      MOZ_BROWSER_TOOLBOX_PORT: String(this.port),
      MOZ_HEADLESS: null,
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

    dump(`Starting Browser Toolbox ${command} ${args.join(" ")}\n`);
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
            dump("> " + data);
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

    if (this.devToolsServer) {
      this.devToolsServer.destroy();
      this.devToolsServer = null;
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
const prefObserver = {
  observe: (...args) => {
    wantLogging = Services.prefs.getBoolPref(args.pop());
  },
};
Services.prefs.addObserver("devtools.debugger.log", prefObserver);
const unloadObserver = function(subject) {
  if (subject.wrappedJSObject == require("@loader/unload")) {
    Services.prefs.removeObserver("devtools.debugger.log", prefObserver);
    Services.obs.removeObserver(unloadObserver, "devtools:loader:destroy");
  }
};
Services.obs.addObserver(unloadObserver, "devtools:loader:destroy");
