/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
const kDebuggerPrefs = [
  "devtools.debugger.remote-enabled",
  "devtools.debugger.chrome-enabled",
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
  },

  handleConsoleFlag: function(cmdLine) {
    let window = Services.wm.getMostRecentWindow("devtools:webconsole");
    if (!window) {
      let devtools = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
      // Load the browser devtools main module as the loader's main module.
      Cu.import("resource:///modules/devtools/gDevTools.jsm");
      let hudservice = devtools.require("devtools/webconsole/hudservice");
      let console = Cu.import("resource://gre/modules/devtools/Console.jsm", {}).console;
      hudservice.toggleBrowserConsole().then(null, console.error);
    } else {
      window.focus(); // the Browser Console was already open
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  handleDebuggerFlag: function(cmdLine) {
    let remoteDebuggingEnabled = false;
    try {
      remoteDebuggingEnabled = kDebuggerPrefs.every((pref) => Services.prefs.getBoolPref(pref));
    } catch (ex) {
      Cu.reportError(ex);
      return;
    }
    if (remoteDebuggingEnabled) {
      Cu.import("resource:///modules/devtools/DebuggerProcess.jsm");
      BrowserDebuggerProcess.init();
    } else {
      let errorMsg = "Could not run chrome debugger! You need the following prefs " +
                     "to be set to true: " + kDebuggerPrefs.join(", ");
      Cu.reportError(errorMsg);
      // Dump as well, as we're doing this from a commandline, make sure people don't miss it:
      dump(errorMsg + "\n");
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  helpInfo : "  -jsconsole         Open the Browser Console.\n" +
             "  -jsdebugger        Open the Browser Debugger.\n",

  classID: Components.ID("{9e9a9283-0ce9-4e4a-8f1c-ba129a032c32}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([devtoolsCommandlineHandler]);
