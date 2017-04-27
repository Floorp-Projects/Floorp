/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This XPCOM component is loaded very early.
 * It handles command line arguments like -jsconsole, but also ensures starting
 * core modules like 'devtools-browser.js' that hooks the browser windows
 * and ensure setting up tools.
 *
 * Be careful to lazy load dependencies as much as possible.
 **/

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
const kDebuggerPrefs = [
  "devtools.debugger.remote-enabled",
  "devtools.chrome.enabled"
];
const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
XPCOMUtils.defineLazyModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");

function DevToolsStartup() {}

DevToolsStartup.prototype = {
  handle: function (cmdLine) {
    let consoleFlag = cmdLine.handleFlag("jsconsole", false);
    let debuggerFlag = cmdLine.handleFlag("jsdebugger", false);
    let devtoolsFlag = cmdLine.handleFlag("devtools", false);

    if (consoleFlag) {
      this.handleConsoleFlag(cmdLine);
    }
    if (debuggerFlag) {
      this.handleDebuggerFlag(cmdLine);
    }
    let debuggerServerFlag;
    try {
      debuggerServerFlag =
        cmdLine.handleFlagWithParam("start-debugger-server", false);
    } catch (e) {
      // We get an error if the option is given but not followed by a value.
      // By catching and trying again, the value is effectively optional.
      debuggerServerFlag = cmdLine.handleFlag("start-debugger-server", false);
    }
    if (debuggerServerFlag) {
      this.handleDebuggerServerFlag(cmdLine, debuggerServerFlag);
    }

    let onStartup = window => {
      Services.obs.removeObserver(onStartup,
                                  "browser-delayed-startup-finished");
      // Ensure loading core module once firefox is ready
      this.initDevTools();

      if (devtoolsFlag) {
        this.handleDevToolsFlag(window);
      }
    };
    Services.obs.addObserver(onStartup, "browser-delayed-startup-finished");
  },

  initDevTools: function () {
    let { loader } = Cu.import("resource://devtools/shared/Loader.jsm", {});
    // Ensure loading main devtools module that hooks up into browser UI
    // and initialize all devtools machinery.
    loader.require("devtools/client/framework/devtools-browser");
  },

  handleConsoleFlag: function (cmdLine) {
    let window = Services.wm.getMostRecentWindow("devtools:webconsole");
    if (!window) {
      this.initDevTools();

      let { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
      let hudservice = require("devtools/client/webconsole/hudservice");
      let { console } = Cu.import("resource://gre/modules/Console.jsm", {});
      hudservice.toggleBrowserConsole().then(null, console.error);
    } else {
      // the Browser Console was already open
      window.focus();
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  // Open the toolbox on the selected tab once the browser starts up.
  handleDevToolsFlag: function (window) {
    const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
    const {gDevTools} = require("devtools/client/framework/devtools");
    const {TargetFactory} = require("devtools/client/framework/target");
    let target = TargetFactory.forTab(window.gBrowser.selectedTab);
    gDevTools.showToolbox(target);
  },

  _isRemoteDebuggingEnabled() {
    let remoteDebuggingEnabled = false;
    try {
      remoteDebuggingEnabled = kDebuggerPrefs.every(pref => {
        return Services.prefs.getBoolPref(pref);
      });
    } catch (ex) {
      let { console } = Cu.import("resource://gre/modules/Console.jsm", {});
      console.error(ex);
      return false;
    }
    if (!remoteDebuggingEnabled) {
      let errorMsg = "Could not run chrome debugger! You need the following " +
                     "prefs to be set to true: " + kDebuggerPrefs.join(", ");
      let { console } = Cu.import("resource://gre/modules/Console.jsm", {});
      console.error(new Error(errorMsg));
      // Dump as well, as we're doing this from a commandline, make sure people
      // don't miss it:
      dump(errorMsg + "\n");
    }
    return remoteDebuggingEnabled;
  },

  handleDebuggerFlag: function (cmdLine) {
    if (!this._isRemoteDebuggingEnabled()) {
      return;
    }

    let devtoolsThreadResumed = false;
    let pauseOnStartup = cmdLine.handleFlag("wait-for-jsdebugger", false);
    if (pauseOnStartup) {
      let observe = function (subject, topic, data) {
        devtoolsThreadResumed = true;
        Services.obs.removeObserver(observe, "devtools-thread-resumed");
      };
      Services.obs.addObserver(observe, "devtools-thread-resumed");
    }

    const { BrowserToolboxProcess } = Cu.import("resource://devtools/client/framework/ToolboxProcess.jsm", {});
    BrowserToolboxProcess.init();

    if (pauseOnStartup) {
      // Spin the event loop until the debugger connects.
      let thread = Cc["@mozilla.org/thread-manager;1"].getService().currentThread;
      while (!devtoolsThreadResumed) {
        thread.processNextEvent(true);
      }
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  /**
   * Handle the --start-debugger-server command line flag. The options are:
   * --start-debugger-server
   *   The portOrPath parameter is boolean true in this case. Reads and uses the defaults
   *   from devtools.debugger.remote-port and devtools.debugger.remote-websocket prefs.
   *   The default values of these prefs are port 6000, WebSocket disabled.
   *
   * --start-debugger-server 6789
   *   Start the non-WebSocket server on port 6789.
   *
   * --start-debugger-server /path/to/filename
   *   Start the server on a Unix domain socket.
   *
   * --start-debugger-server ws:6789
   *   Start the WebSocket server on port 6789.
   *
   * --start-debugger-server ws:
   *   Start the WebSocket server on the default port (taken from d.d.remote-port)
   */
  handleDebuggerServerFlag: function (cmdLine, portOrPath) {
    if (!this._isRemoteDebuggingEnabled()) {
      return;
    }

    let webSocket = false;
    let defaultPort = Services.prefs.getIntPref("devtools.debugger.remote-port");
    if (portOrPath === true) {
      // Default to pref values if no values given on command line
      webSocket = Services.prefs.getBoolPref("devtools.debugger.remote-websocket");
      portOrPath = defaultPort;
    } else if (portOrPath.startsWith("ws:")) {
      webSocket = true;
      let port = portOrPath.slice(3);
      portOrPath = Number(port) ? port : defaultPort;
    }

    let { DevToolsLoader } =
      Cu.import("resource://devtools/shared/Loader.jsm", {});

    try {
      // Create a separate loader instance, so that we can be sure to receive
      // a separate instance of the DebuggingServer from the rest of the
      // devtools.  This allows us to safely use the tools against even the
      // actors and DebuggingServer itself, especially since we can mark
      // serverLoader as invisible to the debugger (unlike the usual loader
      // settings).
      let serverLoader = new DevToolsLoader();
      serverLoader.invisibleToDebugger = true;
      let { DebuggerServer: debuggerServer } =
        serverLoader.require("devtools/server/main");
      debuggerServer.init();
      debuggerServer.addBrowserActors();
      debuggerServer.allowChromeProcess = true;

      let listener = debuggerServer.createListener();
      listener.portOrPath = portOrPath;
      listener.webSocket = webSocket;
      listener.open();
      dump("Started debugger server on " + portOrPath + "\n");
    } catch (e) {
      dump("Unable to start debugger server on " + portOrPath + ": " + e);
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  /* eslint-disable max-len */
  helpInfo: "  --jsconsole        Open the Browser Console.\n" +
            "  --jsdebugger       Open the Browser Toolbox.\n" +
            "  --wait-for-jsdebugger Spin event loop until JS debugger connects.\n" +
            "                     Enables debugging (some) application startup code paths.\n" +
            "                     Only has an effect when `--jsdebugger` is also supplied.\n" +
            "  --devtools         Open DevTools on initial load.\n" +
            "  --start-debugger-server [ws:][ <port> | <path> ] Start the debugger server on\n" +
            "                     a TCP port or Unix domain socket path. Defaults to TCP port\n" +
            "                     6000. Use WebSocket protocol if ws: prefix is specified.\n",
  /* eslint-disable max-len */

  classID: Components.ID("{9e9a9283-0ce9-4e4a-8f1c-ba129a032c32}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory(
  [DevToolsStartup]);
