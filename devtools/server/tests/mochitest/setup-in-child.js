"use strict";

const {Cc} = require("chrome");
const cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"].getService();
const { DebuggerServer } = require("devtools/server/main");

exports.setupChild = function(a, b, c) {
  cpmm.sendAsyncMessage("test:setupChild", [a, b, c]);
};

exports.callParent = function() {
  // Hack! Fetch DebuggerServerConnection objects directly within DebuggerServer guts.
  for (const id in DebuggerServer._connections) {
    const conn = DebuggerServer._connections[id];
    conn.setupInParent({
      module: "chrome://mochitests/content/chrome/devtools/server/tests/mochitest/setup-in-parent.js",
      setupParent: "setupParent",
      args: [{one: true}, 2, "three"]
    });
  }
};
