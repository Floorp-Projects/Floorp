/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_target-from-url.js</p>";

const { DevToolsLoader } = Cu.import("resource://devtools/shared/Loader.jsm", {});
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
  is(target.isTabActor, true);
  is(target.isRemote, true);
}

add_task(function* () {
  let tab = yield addTab(TEST_URI);
  let browser = tab.linkedBrowser;
  let target;

  info("Test invalid type");
  try {
    yield targetFromURL(new URL("http://foo?type=x"));
    ok(false, "Shouldn't pass");
  } catch (e) {
    is(e.message, "targetFromURL, unsupported type 'x' parameter");
  }

  info("Test browser window");
  let windowId = window.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils)
                       .outerWindowID;
  target = yield targetFromURL(new URL("http://foo?type=window&id=" + windowId));
  is(target.url, window.location.href);
  is(target.isLocalTab, false);
  is(target.chrome, true);
  is(target.isTabActor, true);
  is(target.isRemote, true);

  info("Test tab");
  windowId = browser.outerWindowID;
  target = yield targetFromURL(new URL("http://foo?type=tab&id=" + windowId));
  assertIsTabTarget(target, TEST_URI);

  info("Test tab with chrome privileges");
  target = yield targetFromURL(new URL("http://foo?type=tab&id=" + windowId + "&chrome"));
  assertIsTabTarget(target, TEST_URI, true);

  info("Test invalid tab id");
  try {
    yield targetFromURL(new URL("http://foo?type=tab&id=10000"));
    ok(false, "Shouldn't pass");
  } catch (e) {
    is(e.message, "targetFromURL, tab with outerWindowID '10000' doesn't exist");
  }

  info("Test parent process");
  target = yield targetFromURL(new URL("http://foo?type=process"));
  let topWindow = Services.wm.getMostRecentWindow("navigator:browser");
  assertIsTabTarget(target, topWindow.location.href, true);

  yield testRemoteTCP();
  yield testRemoteWebSocket();

  gBrowser.removeCurrentTab();
});

function* setupDebuggerServer(websocket) {
  info("Create a separate loader instance for the DebuggerServer.");
  let loader = new DevToolsLoader();
  let { DebuggerServer } = loader.require("devtools/server/main");

  DebuggerServer.init();
  DebuggerServer.addBrowserActors();
  DebuggerServer.allowChromeProcess = true;

  let listener = DebuggerServer.createListener();
  ok(listener, "Socket listener created");
  // Pass -1 to automatically choose an available port
  listener.portOrPath = -1;
  listener.webSocket = websocket;
  yield listener.open();
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

function* testRemoteTCP() {
  info("Test remote process via TCP Connection");

  let server = yield setupDebuggerServer(false);

  let { port } = server.listener;
  let target = yield targetFromURL(new URL("http://foo?type=process&host=127.0.0.1&port=" + port));
  let topWindow = Services.wm.getMostRecentWindow("navigator:browser");
  assertIsTabTarget(target, topWindow.location.href, true);

  let settings = target.client._transport.connectionSettings;
  is(settings.host, "127.0.0.1");
  is(settings.port, port);
  is(settings.webSocket, false);

  yield target.client.close();

  teardownDebuggerServer(server);
}

function* testRemoteWebSocket() {
  info("Test remote process via WebSocket Connection");

  let server = yield setupDebuggerServer(true);

  let { port } = server.listener;
  let target = yield targetFromURL(new URL("http://foo?type=process&host=127.0.0.1&port=" + port + "&ws=true"));
  let topWindow = Services.wm.getMostRecentWindow("navigator:browser");
  assertIsTabTarget(target, topWindow.location.href, true);

  let settings = target.client._transport.connectionSettings;
  is(settings.host, "127.0.0.1");
  is(settings.port, port);
  is(settings.webSocket, true);
  yield target.client.close();

  teardownDebuggerServer(server);
}
