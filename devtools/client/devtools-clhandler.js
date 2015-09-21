/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
const kDebuggerPrefs = [
  "devtools.debugger.remote-enabled",
  "devtools.chrome.enabled"
];
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");

function devtoolsCommandlineHandler() {
}
devtoolsCommandlineHandler.prototype = {
  handle: function(cmdLine) {
    let consoleFlag = cmdLine.handleFlag("jsconsole", false);
    let debuggerFlag = cmdLine.handleFlag("jsdebugger", false);
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
    } catch(e) {
      // We get an error if the option is given but not followed by a value.
      // By catching and trying again, the value is effectively optional.
      debuggerServerFlag = cmdLine.handleFlag("start-debugger-server", false);
    }
    if (debuggerServerFlag) {
      this.handleDebuggerServerFlag(cmdLine, debuggerServerFlag);
    }
  },

  handleConsoleFlag: function(cmdLine) {
    let window = Services.wm.getMostRecentWindow("devtools:webconsole");
    if (!window) {
      let { require } = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});
      // Load the browser devtools main module as the loader's main module.
      Cu.import("resource:///modules/devtools/client/framework/gDevTools.jsm");
      let hudservice = require("devtools/client/webconsole/hudservice");
      let { console } = Cu.import("resource://gre/modules/devtools/shared/Console.jsm", {});
      hudservice.toggleBrowserConsole().then(null, console.error);
    } else {
      window.focus(); // the Browser Console was already open
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  _isRemoteDebuggingEnabled() {
    let remoteDebuggingEnabled = false;
    try {
      remoteDebuggingEnabled = kDebuggerPrefs.every((pref) => Services.prefs.getBoolPref(pref));
    } catch (ex) {
      Cu.reportError(ex);
      return false;
    }
    if (!remoteDebuggingEnabled) {
      let errorMsg = "Could not run chrome debugger! You need the following prefs " +
                     "to be set to true: " + kDebuggerPrefs.join(", ");
      Cu.reportError(errorMsg);
      // Dump as well, as we're doing this from a commandline, make sure people
      // don't miss it:
      dump(errorMsg + "\n");
    }
    return remoteDebuggingEnabled;
  },

  handleDebuggerFlag: function(cmdLine) {
    if (!this._isRemoteDebuggingEnabled()) {
      return;
    }
    Cu.import("resource:///modules/devtools/client/framework/ToolboxProcess.jsm");
    BrowserToolboxProcess.init();

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  handleDebuggerServerFlag: function(cmdLine, portOrPath) {
    if (!this._isRemoteDebuggingEnabled()) {
      return;
    }
    if (portOrPath === true) {
      // Default to TCP port 6000 if no value given
      portOrPath = 6000;
    }
    let { DevToolsLoader } =
      Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});

    try {
      // Create a separate loader instance, so that we can be sure to receive
      // a separate instance of the DebuggingServer from the rest of the
      // devtools.  This allows us to safely use the tools against even the
      // actors and DebuggingServer itself, especially since we can mark
      // serverLoader as invisible to the debugger (unlike the usual loader
      // settings).
      let serverLoader = new DevToolsLoader();
      serverLoader.invisibleToDebugger = true;
      serverLoader.main("devtools/server/main");
      let debuggerServer = serverLoader.DebuggerServer;
      debuggerServer.init();
      debuggerServer.addBrowserActors();
      debuggerServer.allowChromeProcess = true;

      let listener = debuggerServer.createListener();
      listener.portOrPath = portOrPath;
      listener.open();
      dump("Started debugger server on " + portOrPath + "\n");
    } catch(e) {
      dump("Unable to start debugger server on " + portOrPath + ": " + e);
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  helpInfo : "  --jsconsole        Open the Browser Console.\n" +
             "  --jsdebugger       Open the Browser Toolbox.\n" +
             "  --start-debugger-server [port|path] " +
             "Start the debugger server on a TCP port or " +
             "Unix domain socket path.  Defaults to TCP port 6000.\n",

  classID: Components.ID("{9e9a9283-0ce9-4e4a-8f1c-ba129a032c32}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([devtoolsCommandlineHandler]);
