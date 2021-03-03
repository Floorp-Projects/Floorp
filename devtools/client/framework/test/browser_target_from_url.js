/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI =
  "data:text/html;charset=utf-8," + "<p>browser_target-from-url.js</p>";

const { DevToolsLoader } = ChromeUtils.import(
  "resource://devtools/shared/Loader.jsm"
);
const {
  descriptorFromURL,
} = require("devtools/client/framework/descriptor-from-url");

Services.prefs.setBoolPref("devtools.debugger.remote-enabled", true);
Services.prefs.setBoolPref("devtools.debugger.prompt-connection", false);

SimpleTest.registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.debugger.remote-enabled");
  Services.prefs.clearUserPref("devtools.debugger.prompt-connection");
});

function assertTarget(target, url, chrome = false) {
  is(target.url, url);
  is(target.isLocalTab, false);
  is(target.chrome, chrome);
  is(target.isBrowsingContext, true);
}

add_task(async function() {
  const tab = await addTab(TEST_URI);
  const browser = tab.linkedBrowser;
  let descriptor, target;

  info("Test invalid type");
  try {
    await descriptorFromURL(new URL("http://foo?type=x"));
    ok(false, "Shouldn't pass");
  } catch (e) {
    is(e.message, "descriptorFromURL, unsupported type 'x' parameter");
  }

  info("Test tab");
  let windowId = window.docShell.outerWindowID;
  windowId = browser.outerWindowID;
  descriptor = await descriptorFromURL(
    new URL("http://foo?type=tab&id=" + windowId)
  );
  target = await descriptor.getTarget();
  assertTarget(target, TEST_URI);
  await target.client.close();

  info("Test invalid tab id");
  try {
    await descriptorFromURL(new URL("http://foo?type=tab&id=10000"));
    ok(false, "Shouldn't pass");
  } catch (e) {
    is(
      e.message,
      "descriptorFromURL, tab with outerWindowID '10000' doesn't exist"
    );
  }

  info("Test parent process");
  descriptor = await descriptorFromURL(new URL("http://foo?type=process"));
  target = await descriptor.getTarget();
  const topWindow = Services.wm.getMostRecentWindow("navigator:browser");
  assertTarget(target, topWindow.location.href, true);
  await target.client.close();

  await testRemoteTCP();
  await testRemoteWebSocket();

  gBrowser.removeCurrentTab();
});

async function setupDevToolsServer(webSocket) {
  info("Create a separate loader instance for the DevToolsServer.");
  const loader = new DevToolsLoader();
  const { DevToolsServer } = loader.require("devtools/server/devtools-server");
  const { SocketListener } = loader.require("devtools/shared/security/socket");

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
  const descriptor = await descriptorFromURL(
    new URL("http://foo?type=process&host=127.0.0.1&port=" + port)
  );
  const target = await descriptor.getTarget();
  const topWindow = Services.wm.getMostRecentWindow("navigator:browser");
  assertTarget(target, topWindow.location.href, true);

  const settings = target.client._transport.connectionSettings;
  is(settings.host, "127.0.0.1");
  is(parseInt(settings.port, 10), port);
  is(settings.webSocket, false);

  await target.client.close();

  teardownDevToolsServer(server);
}

async function testRemoteWebSocket() {
  info("Test remote process via WebSocket Connection");

  const server = await setupDevToolsServer(true);

  const { port } = server.listener;
  const descriptor = await descriptorFromURL(
    new URL("http://foo?type=process&host=127.0.0.1&port=" + port + "&ws=true")
  );
  const target = await descriptor.getTarget();
  const topWindow = Services.wm.getMostRecentWindow("navigator:browser");
  assertTarget(target, topWindow.location.href, true);

  const settings = target.client._transport.connectionSettings;
  is(settings.host, "127.0.0.1");
  is(parseInt(settings.port, 10), port);
  is(settings.webSocket, true);
  await target.client.close();

  teardownDevToolsServer(server);
}
