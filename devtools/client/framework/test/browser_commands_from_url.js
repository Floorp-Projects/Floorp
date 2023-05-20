/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI =
  "data:text/html;charset=utf-8," + "<p>browser_target-from-url.js</p>";

const { DevToolsLoader } = ChromeUtils.importESModule(
  "resource://devtools/shared/loader/Loader.sys.mjs"
);
const {
  commandsFromURL,
} = require("resource://devtools/client/framework/commands-from-url.js");

Services.prefs.setBoolPref("devtools.debugger.remote-enabled", true);
Services.prefs.setBoolPref("devtools.debugger.prompt-connection", false);

SimpleTest.registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.debugger.remote-enabled");
  Services.prefs.clearUserPref("devtools.debugger.prompt-connection");
});

function assertTarget(target, url) {
  is(target.url, url);
  is(target.isBrowsingContext, true);
}

add_task(async function () {
  const tab = await addTab(TEST_URI);
  const browser = tab.linkedBrowser;
  let commands, target;

  info("Test invalid type");
  try {
    await commandsFromURL(new URL("https://foo?type=x"));
    ok(false, "Shouldn't pass");
  } catch (e) {
    is(e.message, "commandsFromURL, unsupported type 'x' parameter");
  }

  info("Test tab");
  commands = await commandsFromURL(
    new URL("https://foo?type=tab&id=" + browser.browserId)
  );
  // Descriptor's getTarget will only work if the TargetCommand watches for the first top target
  await commands.targetCommand.startListening();

  // For now, we can't spawn a commands flagged as 'local tab' via URL query params
  // The only way to has isLocalTab is to create the toolbox via showToolboxForTab
  // and spawn the command via CommandsFactory.forTab.
  is(
    commands.descriptorFront.isLocalTab,
    false,
    "Even if we refer to a local tab, isLocalTab is false (for now)"
  );

  target = await commands.descriptorFront.getTarget();

  assertTarget(target, TEST_URI);
  await commands.destroy();

  info("Test invalid tab id");
  try {
    await commandsFromURL(new URL("https://foo?type=tab&id=10000"));
    ok(false, "Shouldn't pass");
  } catch (e) {
    is(e.message, "commandsFromURL, tab with browserId '10000' doesn't exist");
  }

  info("Test parent process");
  commands = await commandsFromURL(new URL("https://foo?type=process"));
  target = await commands.descriptorFront.getTarget();
  const topWindow = Services.wm.getMostRecentWindow("navigator:browser");
  assertTarget(target, topWindow.location.href);
  await commands.destroy();

  await testRemoteTCP();
  await testRemoteWebSocket();

  gBrowser.removeCurrentTab();
});

async function setupDevToolsServer(webSocket) {
  info("Create a separate loader instance for the DevToolsServer.");
  const loader = new DevToolsLoader();
  const { DevToolsServer } = loader.require(
    "resource://devtools/server/devtools-server.js"
  );
  const { SocketListener } = loader.require(
    "resource://devtools/shared/security/socket.js"
  );

  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  DevToolsServer.allowChromeProcess = true;
  const socketOptions = {
    // Pass -1 to automatically choose an available port
    portOrPath: -1,
    webSocket,
  };

  const listener = new SocketListener(DevToolsServer, socketOptions);
  ok(listener, "Socket listener created");
  await listener.open();
  is(DevToolsServer.listeningSockets, 1, "1 listening socket");

  return { DevToolsServer, listener };
}

function teardownDevToolsServer({ DevToolsServer, listener }) {
  info("Close the listener socket");
  listener.close();
  is(DevToolsServer.listeningSockets, 0, "0 listening sockets");

  info("Destroy the temporary devtools server");
  DevToolsServer.destroy();
}

async function testRemoteTCP() {
  info("Test remote process via TCP Connection");

  const server = await setupDevToolsServer(false);

  const { port } = server.listener;
  const commands = await commandsFromURL(
    new URL("https://foo?type=process&host=127.0.0.1&port=" + port)
  );
  const target = await commands.descriptorFront.getTarget();
  const topWindow = Services.wm.getMostRecentWindow("navigator:browser");
  assertTarget(target, topWindow.location.href);

  const settings = commands.client._transport.connectionSettings;
  is(settings.host, "127.0.0.1");
  is(parseInt(settings.port, 10), port);
  is(settings.webSocket, false);

  await commands.destroy();

  teardownDevToolsServer(server);
}

async function testRemoteWebSocket() {
  info("Test remote process via WebSocket Connection");

  const server = await setupDevToolsServer(true);

  const { port } = server.listener;
  const commands = await commandsFromURL(
    new URL("https://foo?type=process&host=127.0.0.1&port=" + port + "&ws=true")
  );
  const target = await commands.descriptorFront.getTarget();
  const topWindow = Services.wm.getMostRecentWindow("navigator:browser");
  assertTarget(target, topWindow.location.href);

  const settings = commands.client._transport.connectionSettings;
  is(settings.host, "127.0.0.1");
  is(parseInt(settings.port, 10), port);
  is(settings.webSocket, true);
  await commands.destroy();

  teardownDevToolsServer(server);
}
