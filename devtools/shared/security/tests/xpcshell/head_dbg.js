/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* exported defer, DevToolsClient, initTestDevToolsServer */

const { loader, require } = ChromeUtils.import(
  "resource://devtools/shared/Loader.jsm"
);
const Services = require("Services");
const xpcInspector = require("xpcInspector");
const { DevToolsServer } = require("devtools/server/devtools-server");
const { DevToolsClient } = require("devtools/client/devtools-client");
// We need to require lazily since will be crashed if we load SocketListener too early
// in xpc shell test due to SocketListener loads PSM module.
loader.lazyRequireGetter(
  this,
  "SocketListener",
  "devtools/shared/security/socket",
  true
);

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

// Convert an nsIScriptError 'logLevel' value into an appropriate string.
function scriptErrorLogLevel(message) {
  switch (message.logLevel) {
    case Ci.nsIConsoleMessage.info:
      return "info";
    case Ci.nsIConsoleMessage.warn:
      return "warning";
    default:
      Assert.equal(message.logLevel, Ci.nsIConsoleMessage.error);
      return "error";
  }
}

// Register a console listener, so console messages don't just disappear
// into the ether.
var listener = {
  observe: function(message) {
    let string;
    try {
      message.QueryInterface(Ci.nsIScriptError);
      dump(
        message.sourceName +
          ":" +
          message.lineNumber +
          ": " +
          scriptErrorLogLevel(message) +
          ": " +
          message.errorMessage +
          "\n"
      );
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

    info("head_dbg.js got console message: " + string + "\n");
  },
};

Services.console.registerListener(listener);

/**
 * Initialize the testing devtools server.
 */
function initTestDevToolsServer() {
  const { createRootActor } = require("xpcshell-test/testactors");
  DevToolsServer.setRootActor(createRootActor);
  DevToolsServer.init();
}
