/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 1181100 - Test DevToolsServerConnection.setupInParent and DevToolsServer.setupInChild
 */

"use strict";

const {
  connectToFrame,
} = require("devtools/server/connectors/frame-connector");

add_task(async () => {
  // Create a minimal browser with a message manager
  const browser = document.createXULElement("browser");
  browser.setAttribute("type", "content");
  document.body.appendChild(browser);

  // Instantiate a minimal server
  DevToolsServer.init();
  if (!DevToolsServer.createRootActor) {
    DevToolsServer.registerAllActors();
  }

  // Fake a connection to an iframe
  const transport = DevToolsServer.connectPipe();
  const conn = transport._serverConnection;
  const client = new DevToolsClient(transport);

  // Wait for a response from setupInChild
  const onChild = msg => {
    Services.ppmm.removeMessageListener("test:setupChild", onChild);
    const args = msg.json;

    is(args[0], 1, "Got first numeric argument");
    is(args[1], "two", "Got second string argument");
    is(args[2].three, true, "Got last JSON argument");

    // Ask the child to call setupInParent
    DevToolsServer.setupInChild({
      module:
        "chrome://mochitests/content/browser/devtools/server/tests/browser/setup-in-child.js",
      setupChild: "callParent",
    });
  };
  Services.ppmm.addMessageListener("test:setupChild", onChild);

  // Wait also for a reponse from setupInParent called from setup-in-child.js
  const onDone = new Promise(resolve => {
    const onParent = (_, topic, args) => {
      Services.obs.removeObserver(onParent, "test:setupParent");
      args = JSON.parse(args);

      is(args[0], true, "Got `mm` argument, a message manager");
      ok(args[1].match(/server\d+.conn\d+.child\d+/), "Got `prefix` argument");

      resolve();
    };
    Services.obs.addObserver(onParent, "test:setupParent");
  });

  // Instanciate e10s machinery and call setupInChild
  await connectToFrame(conn, browser);
  DevToolsServer.setupInChild({
    module:
      "chrome://mochitests/content/browser/devtools/server/tests/browser/setup-in-child.js",
    setupChild: "setupChild",
    args: [1, "two", { three: true }],
  });
  await onDone;

  await client.close();
  DevToolsServer.destroy();
  browser.remove();
});
