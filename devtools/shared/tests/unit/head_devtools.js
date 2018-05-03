/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* exported DevToolsUtils, DevToolsLoader */

"use strict";

const { require, DevToolsLoader } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const Services = require("Services");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

Services.prefs.setBoolPref("devtools.testing", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.testing");
});

// Register a console listener, so console messages don't just disappear
// into the ether.

// If for whatever reason the test needs to post console errors that aren't
// failures, set this to true.
var ALLOW_CONSOLE_ERRORS = false;

// XXX This listener is broken, see bug 1456634, for now turn off no-undef here,
// this needs turning back on!
/* eslint-disable no-undef */
var listener = {
  observe: function(message) {
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
    while (DebuggerServer.xpcInspector.eventLoopNestLevel > 0) {
      DebuggerServer.xpcInspector.exitNestedEventLoop();
    }

    if (!ALLOW_CONSOLE_ERRORS) {
      do_throw("head_devtools.js got console message: " + string + "\n");
    }
  }
};
/* eslint-enable no-undef */

Services.console.registerListener(listener);
