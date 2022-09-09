/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* exported Cr, CC, NetUtil, errorCount, initTestDevToolsServer,
            writeTestTempFile, socket_transport, local_transport, really_long
*/

var CC = Components.Constructor;

const { require } = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);
const { NetUtil } = require("resource://gre/modules/NetUtil.jsm");

// We do not want to log packets by default, because in some tests,
// we can be sending large amounts of data. The test harness has
// trouble dealing with logging all the data, and we end up with
// intermittent time outs (e.g. bug 775924).
// Services.prefs.setBoolPref("devtools.debugger.log", true);
// Services.prefs.setBoolPref("devtools.debugger.log.verbose", true);
// Enable remote debugging for the relevant tests.
Services.prefs.setBoolPref("devtools.debugger.remote-enabled", true);

const {
  ActorRegistry,
} = require("devtools/server/actors/utils/actor-registry");
const { DevToolsServer } = require("devtools/server/devtools-server");
const { DevToolsClient } = require("devtools/client/devtools-client");
const { SocketListener } = require("devtools/shared/security/socket");

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
var errorCount = 0;
var listener = {
  observe(message) {
    errorCount++;
    let string = "";
    try {
      // If we've been given an nsIScriptError, then we can print out
      // something nicely formatted, for tools like Emacs to pick up.
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
    while (DevToolsServer.xpcInspector.eventLoopNestLevel > 0) {
      DevToolsServer.xpcInspector.exitNestedEventLoop();
    }

    do_throw("head_dbg.js got console message: " + string + "\n");
  },
};

Services.console.registerListener(listener);

/**
 * Initialize the testing devtools server.
 */
function initTestDevToolsServer() {
  ActorRegistry.registerModule("devtools/server/actors/thread", {
    prefix: "script",
    constructor: "ScriptActor",
    type: { global: true, target: true },
  });
  const { createRootActor } = require("xpcshell-test/testactors");
  DevToolsServer.setRootActor(createRootActor);
  // Allow incoming connections.
  DevToolsServer.init();
  // Avoid the server from being destroyed when the last connection closes
  DevToolsServer.keepAlive = true;
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
  const stream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  stream.init(file, -1, -1, 0);
  try {
    do {
      const numWritten = stream.write(content, content.length);
      content = content.slice(numWritten);
    } while (content.length);
  } finally {
    stream.close();
  }
}

/** * Transport Factories ***/

var socket_transport = async function() {
  if (!DevToolsServer.listeningSockets) {
    const AuthenticatorType = DevToolsServer.Authenticators.get("PROMPT");
    const authenticator = new AuthenticatorType.Server();
    authenticator.allowConnection = () => {
      return DevToolsServer.AuthenticationResult.ALLOW;
    };
    const socketOptions = {
      authenticator,
      portOrPath: -1,
    };
    const debuggerListener = new SocketListener(DevToolsServer, socketOptions);
    await debuggerListener.open();
  }
  const port = DevToolsServer._listeners[0].port;
  info("DevTools server port is " + port);
  return DevToolsClient.socketConnect({ host: "127.0.0.1", port });
};

function local_transport() {
  return Promise.resolve(DevToolsServer.connectPipe());
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
