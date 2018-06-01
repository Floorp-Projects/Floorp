/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* exported Cr, CC, NetUtil, defer, errorCount, initTestDebuggerServer,
            writeTestTempFile, socket_transport, local_transport, really_long
*/

var CC = Components.Constructor;

const { require } =
  ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const { NetUtil } = require("resource://gre/modules/NetUtil.jsm");
const promise = require("promise");
const defer = require("devtools/shared/defer");

const Services = require("Services");

// We do not want to log packets by default, because in some tests,
// we can be sending large amounts of data. The test harness has
// trouble dealing with logging all the data, and we end up with
// intermittent time outs (e.g. bug 775924).
// Services.prefs.setBoolPref("devtools.debugger.log", true);
// Services.prefs.setBoolPref("devtools.debugger.log.verbose", true);
// Enable remote debugging for the relevant tests.
Services.prefs.setBoolPref("devtools.debugger.remote-enabled", true);

const { DebuggerServer } = require("devtools/server/main");
const { DebuggerClient } = require("devtools/shared/client/debugger-client");

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
var errorCount = 0;
var listener = {
  observe: function(message) {
    errorCount++;
    let string = "";
    try {
      // If we've been given an nsIScriptError, then we can print out
      // something nicely formatted, for tools like Emacs to pick up.
      message.QueryInterface(Ci.nsIScriptError);
      dump(message.sourceName + ":" + message.lineNumber + ": " +
           scriptErrorFlagsToKind(message.flags) + ": " +
           message.errorMessage + "\n");
      string = message.errorMessage;
    } catch (x) {
      // Be a little paranoid with message, as the whole goal here is to lose
      // no information.
      try {
        string = message.message;
      } catch (e) {
        string = "<error converting error message to string>";
      }
    }

    // Make sure we exit all nested event loops so that the test can finish.
    while (DebuggerServer.xpcInspector.eventLoopNestLevel > 0) {
      DebuggerServer.xpcInspector.exitNestedEventLoop();
    }

    // Throw in most cases, but ignore the "strict" messages
    if (!(message.flags & Ci.nsIScriptError.strictFlag)) {
      do_throw("head_dbg.js got console message: " + string + "\n");
    }
  }
};

Services.console.registerListener(listener);

/**
 * Initialize the testing debugger server.
 */
function initTestDebuggerServer() {
  DebuggerServer.registerModule("devtools/server/actors/thread", {
    prefix: "script",
    constructor: "ScriptActor",
    type: { global: true, tab: true }
  });
  DebuggerServer.registerModule("xpcshell-test/testactors");
  // Allow incoming connections.
  DebuggerServer.init();
}

/**
 * Wrapper around do_get_file to prefix files with the name of current test to
 * avoid collisions when running in parallel.
 */
function getTestTempFile(fileName, allowMissing) {
  let thisTest = _TEST_FILE.toString().replace(/\\/g, "/");
  thisTest = thisTest.substring(thisTest.lastIndexOf("/") + 1);
  thisTest = thisTest.replace(/\..*$/, "");
  return do_get_file(fileName + "-" + thisTest, allowMissing);
}

function writeTestTempFile(fileName, content) {
  const file = getTestTempFile(fileName, true);
  const stream = Cc["@mozilla.org/network/file-output-stream;1"]
    .createInstance(Ci.nsIFileOutputStream);
  stream.init(file, -1, -1, 0);
  try {
    do {
      const numWritten = stream.write(content, content.length);
      content = content.slice(numWritten);
    } while (content.length > 0);
  } finally {
    stream.close();
  }
}

/** * Transport Factories ***/

var socket_transport = async function() {
  if (!DebuggerServer.listeningSockets) {
    const AuthenticatorType = DebuggerServer.Authenticators.get("PROMPT");
    const authenticator = new AuthenticatorType.Server();
    authenticator.allowConnection = () => {
      return DebuggerServer.AuthenticationResult.ALLOW;
    };
    const debuggerListener = DebuggerServer.createListener();
    debuggerListener.portOrPath = -1;
    debuggerListener.authenticator = authenticator;
    await debuggerListener.open();
  }
  const port = DebuggerServer._listeners[0].port;
  info("Debugger server port is " + port);
  return DebuggerClient.socketConnect({ host: "127.0.0.1", port });
};

function local_transport() {
  return promise.resolve(DebuggerServer.connectPipe());
}

/** * Sample Data ***/

var gReallyLong;
function really_long() {
  if (gReallyLong) {
    return gReallyLong;
  }
  let ret = "0123456789";
  for (let i = 0; i < 18; i++) {
    ret += ret;
  }
  gReallyLong = ret;
  return ret;
}
