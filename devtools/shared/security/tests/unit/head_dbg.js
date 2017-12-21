/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* exported defer, DebuggerClient, initTestDebuggerServer */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
const { require } =
  Cu.import("resource://devtools/shared/Loader.jsm", {});
const defer = require("devtools/shared/defer");
const Services = require("Services");
const xpcInspector = require("xpcInspector");
const { DebuggerServer } = require("devtools/server/main");
const { DebuggerClient } = require("devtools/shared/client/debugger-client");

// We do not want to log packets by default, because in some tests,
// we can be sending large amounts of data. The test harness has
// trouble dealing with logging all the data, and we end up with
// intermittent time outs (e.g. bug 775924).
// Services.prefs.setBoolPref("devtools.debugger.log", true);
// Services.prefs.setBoolPref("devtools.debugger.log.verbose", true);
// Enable remote debugging for the relevant tests.
Services.prefs.setBoolPref("devtools.debugger.remote-enabled", true);
// Fast timeout for TLS tests
Services.prefs.setIntPref("devtools.remote.tls-handshake-timeout", 1000);

// Convert an nsIScriptError 'flags' value into an appropriate string.
function scriptErrorFlagsToKind(flags) {
  let kind;
  if (flags & Ci.nsIScriptError.warningFlag) {
    kind = "warning";
  }
  if (flags & Ci.nsIScriptError.exceptionFlag) {
    kind = "exception";
  } else {
    kind = "error";
  }

  if (flags & Ci.nsIScriptError.strictFlag) {
    kind = "strict " + kind;
  }

  return kind;
}

// Register a console listener, so console messages don't just disappear
// into the ether.
var listener = {
  observe: function (message) {
    let string;
    try {
      message.QueryInterface(Ci.nsIScriptError);
      dump(message.sourceName + ":" + message.lineNumber + ": " +
           scriptErrorFlagsToKind(message.flags) + ": " +
           message.errorMessage + "\n");
      string = message.errorMessage;
    } catch (ex) {
      // Be a little paranoid with message, as the whole goal here is to lose
      // no information.
      try {
        string = "" + message.message;
      } catch (e) {
        string = "<error converting error message to string>";
      }
    }

    // Make sure we exit all nested event loops so that the test can finish.
    while (xpcInspector.eventLoopNestLevel > 0) {
      xpcInspector.exitNestedEventLoop();
    }

    // Print in most cases, but ignore the "strict" messages
    if (!(message.flags & Ci.nsIScriptError.strictFlag)) {
      info("head_dbg.js got console message: " + string + "\n");
    }
  }
};

var consoleService = Cc["@mozilla.org/consoleservice;1"]
                     .getService(Ci.nsIConsoleService);
consoleService.registerListener(listener);

/**
 * Initialize the testing debugger server.
 */
function initTestDebuggerServer() {
  DebuggerServer.registerModule("xpcshell-test/testactors");
  DebuggerServer.init();
}
