/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { DevToolsLoader } = ChromeUtils.import(
  "resource://devtools/shared/Loader.jsm"
);
const { DevToolsSocketStatus } = ChromeUtils.import(
  "resource://devtools/shared/security/DevToolsSocketStatus.jsm"
);

add_task(async function() {
  Services.prefs.setBoolPref("devtools.debugger.remote-enabled", true);
  Services.prefs.setBoolPref("devtools.debugger.prompt-connection", false);

  info("Without any server started, status should be set to false");
  checkSocketStatus(false);

  info("Start a first server, expect status to change to true");
  const server = await setupDevToolsServer({ fromBrowserToolbox: false });
  checkSocketStatus(true);

  info("Start another server, expect status to remain true");
  const otherServer = await setupDevToolsServer({ fromBrowserToolbox: false });
  checkSocketStatus(true);

  info("Shutdown one of the servers, expect status to remain true");
  teardownDevToolsServer(otherServer);
  checkSocketStatus(true);

  info("Shutdown the other server, expect status to change to false");
  teardownDevToolsServer(server);
  checkSocketStatus(false);

  info("Start a 'browser toolbox' server, expect status to remain to false");
  const browserToolboxServer = await setupDevToolsServer({
    fromBrowserToolbox: true,
  });
  checkSocketStatus(false);

  info(
    "Shutdown the 'browser toolbox' server, expect status to remain to false"
  );
  teardownDevToolsServer(browserToolboxServer);
  checkSocketStatus(false);

  Services.prefs.clearUserPref("devtools.debugger.remote-enabled");
  Services.prefs.clearUserPref("devtools.debugger.prompt-connection");
});

function checkSocketStatus(expected) {
  const opened = DevToolsSocketStatus.opened;
  if (expected) {
    ok(opened, "DevTools socket is opened for connections");
  } else {
    ok(!opened, "No DevTools socket is opened for connections");
  }
}

async function setupDevToolsServer({ fromBrowserToolbox }) {
  info("Create a separate loader instance for the DevToolsServer.");
  const loader = new DevToolsLoader();
  const { DevToolsServer } = loader.require("devtools/server/devtools-server");

  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  DevToolsServer.allowChromeProcess = true;
  const socketOptions = {
    fromBrowserToolbox,
    // Pass -1 to automatically choose an available port
    portOrPath: -1,
  };

  const listener = new SocketListener(DevToolsServer, socketOptions);
  ok(listener, "Socket listener created");
  await listener.open();
  equal(DevToolsServer.listeningSockets, 1, "1 listening socket");

  return { DevToolsServer, listener };
}

function teardownDevToolsServer({ DevToolsServer, listener }) {
  info("Close the listener socket");
  listener.close();
  equal(DevToolsServer.listeningSockets, 0, "0 listening sockets");

  info("Destroy the temporary devtools server");
  DevToolsServer.destroy();
}
