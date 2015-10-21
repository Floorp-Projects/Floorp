"use strict";
var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

var {require} = Cu.import("resource://devtools/shared/Loader.jsm");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

// Register a console listener, so console messages don't just disappear
// into the ether.

// If for whatever reason the test needs to post console errors that aren't
// failures, set this to true.
var ALLOW_CONSOLE_ERRORS = false;

var errorCount = 0;
var listener = {
  observe: function (aMessage) {
    errorCount++;
    try {
      // If we've been given an nsIScriptError, then we can print out
      // something nicely formatted, for tools like Emacs to pick up.
      var scriptError = aMessage.QueryInterface(Ci.nsIScriptError);
      dump(aMessage.sourceName + ":" + aMessage.lineNumber + ": " +
           scriptErrorFlagsToKind(aMessage.flags) + ": " +
           aMessage.errorMessage + "\n");
      var string = aMessage.errorMessage;
    } catch (x) {
      // Be a little paranoid with message, as the whole goal here is to lose
      // no information.
      try {
        var string = "" + aMessage.message;
      } catch (x) {
        var string = "<error converting error message to string>";
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

var consoleService = Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService);
consoleService.registerListener(listener);
