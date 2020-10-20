"use strict";

const { Cc } = require("chrome");
const cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"].getService();
const { DevToolsServer } = require("devtools/server/devtools-server");

exports.setupChild = function(a, b, c) {
  cpmm.sendAsyncMessage("test:setupChild", [a, b, c]);
};

exports.callParent = function() {
  // Hack! Fetch DevToolsServerConnection objects directly within DevToolsServer guts.
  for (const id in DevToolsServer._connections) {
    const conn = DevToolsServer._connections[id];
    // eslint-disable-next-line no-restricted-properties
    conn.setupInParent({
      module:
        "chrome://mochitests/content/browser/devtools/server/tests/browser/setup-in-parent.js",
      setupParent: "setupParent",
      args: [{ one: true }, 2, "three"],
    });
  }
};
