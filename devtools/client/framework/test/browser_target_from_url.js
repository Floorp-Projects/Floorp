/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_target-from-url.js</p>";

const { DevToolsLoader } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const { targetFromURL } = require("devtools/client/framework/target-from-url");

Services.prefs.setBoolPref("devtools.debugger.remote-enabled", true);
Services.prefs.setBoolPref("devtools.debugger.prompt-connection", false);

SimpleTest.registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.debugger.remote-enabled");
  Services.prefs.clearUserPref("devtools.debugger.prompt-connection");
});

function assertIsTabTarget(target, url, chrome = false) {
  is(target.url, url);
  is(target.isLocalTab, false);
  is(target.chrome, chrome);
  is(target.isBrowsingContext, true);
  is(target.isRemote, true);
}

add_task(async function() {
  const tab = await addTab(TEST_URI);
  const browser = tab.linkedBrowser;
  let target;

  info("Test invalid type");
  try {
    await targetFromURL(new URL("http://foo?type=x"));
    ok(false, "Shouldn't pass");
  } catch (e) {
    is(e.message, "targetFromURL, unsupported type 'x' parameter");
  }

  info("Test browser window");
  let windowId = window.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils)
                       .outerWindowID;
  target = await targetFromURL(new URL("http://foo?type=window&id=" + windowId));
  is(target.url, window.location.href);
  is(target.isLocalTab, false);
  is(target.chrome, true);
  is(target.isBrowsingContext, true);
  is(target.isRemote, true);

  info("Test tab");
  windowId = browser.outerWindowID;
  target = await targetFromURL(new URL("http://foo?type=tab&id=" + windowId));
  assertIsTabTarget(target, TEST_URI);

  info("Test tab with chrome privileges");
  target = await targetFromURL(new URL("http://foo?type=tab&id=" + windowId + "&chrome"));
  assertIsTabTarget(target, TEST_URI, true);

  info("Test invalid tab id");
  try {
    await targetFromURL(new URL("http://foo?type=tab&id=10000"));
    ok(false, "Shouldn't pass");
  } catch (e) {
    is(e.message, "targetFromURL, tab with outerWindowID '10000' doesn't exist");
  }

  info("Test parent process");
  target = await targetFromURL(new URL("http://foo?type=process"));
  const topWindow = Services.wm.getMostRecentWindow("navigator:browser");
  assertIsTabTarget(target, topWindow.location.href, true);

  await testRemoteTCP();
  await testRemoteWebSocket();

  gBrowser.removeCurrentTab();
});

async function setupDebuggerServer(websocket) {
  info("Create a separate loader instance for the DebuggerServer.");
  const loader = new DevToolsLoader();
  const { DebuggerServer } = loader.require("devtools/server/main");

  DebuggerServer.init();
  DebuggerServer.registerAllActors();
  DebuggerServer.allowChromeProcess = true;

  const listener = DebuggerServer.createListener();
  ok(listener, "Socket listener created");
  // Pass -1 to automatically choose an available port
  listener.portOrPath = -1;
  listener.webSocket = websocket;
  await listener.open();
  is(DebuggerServer.listeningSockets, 1, "1 listening socket");

  return { DebuggerServer, listener };
}

function teardownDebuggerServer({ DebuggerServer, listener }) {
  info("Close the listener socket");
  listener.close();
  is(DebuggerServer.listeningSockets, 0, "0 listening sockets");

  info("Destroy the temporary debugger server");
  DebuggerServer.destroy();
}

async function testRemoteTCP() {
  info("Test remote process via TCP Connection");

  const server = await setupDebuggerServer(false);

  const { port } = server.listener;
  const target = await targetFromURL(new URL("http://foo?type=process&host=127.0.0.1&port=" + port));
  const topWindow = Services.wm.getMostRecentWindow("navigator:browser");
  assertIsTabTarget(target, topWindow.location.href, true);

  const settings = target.client._transport.connectionSettings;
  is(settings.host, "127.0.0.1");
  is(settings.port, port);
  is(settings.webSocket, false);

  await target.client.close();

  teardownDebuggerServer(server);
}

async function testRemoteWebSocket() {
  info("Test remote process via WebSocket Connection");

  const server = await setupDebuggerServer(true);

  const { port } = server.listener;
  const target = await targetFromURL(new URL("http://foo?type=process&host=127.0.0.1&port=" + port + "&ws=true"));
  const topWindow = Services.wm.getMostRecentWindow("navigator:browser");
  assertIsTabTarget(target, topWindow.location.href, true);

  const settings = target.client._transport.connectionSettings;
  is(settings.host, "127.0.0.1");
  is(settings.port, port);
  is(settings.webSocket, true);
  await target.client.close();

  teardownDebuggerServer(server);
}
