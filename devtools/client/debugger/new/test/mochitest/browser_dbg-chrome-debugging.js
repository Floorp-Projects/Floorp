/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that chrome debugging works.
 */

var gClient, gThreadClient;
var gNewGlobal = promise.defer();
var gNewChromeSource = promise.defer();

var { DevToolsLoader } = Cu.import("resource://devtools/shared/Loader.jsm", {});
var customLoader = new DevToolsLoader();
customLoader.invisibleToDebugger = true;
var { DebuggerServer } = customLoader.require("devtools/server/main");
var { DebuggerClient } = require("devtools/shared/client/main");

function initDebuggerClient() {
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }
  DebuggerServer.allowChromeProcess = true;

  let transport = DebuggerServer.connectPipe();
  return new DebuggerClient(transport);
}

function attachThread(client, actor) {
  return new Promise(resolve => {
    client.attachTab(actor, (response, tabClient) => {
      tabClient.attachThread(null, (r, threadClient) => {
        resolve(threadClient);
      });
    });
  });
}

function onNewGlobal() {
  ok(true, "Received a new chrome global.");
  gClient.removeListener("newGlobal", onNewGlobal);
  gNewGlobal.resolve();
}

function onNewSource(event, packet) {
  if (packet.source.url.startsWith("chrome:")) {
    ok(true, "Received a new chrome source: " + packet.source.url);
    gThreadClient.removeListener("newSource", onNewSource);
    gNewChromeSource.resolve();
  }
}

function resumeAndCloseConnection() {
  return new Promise(resolve => {
    gThreadClient.resume(() => resolve(gClient.close()));
  });
}

registerCleanupFunction(function() {
  gClient = null;
  gThreadClient = null;
  gNewGlobal = null;
  gNewChromeSource = null;

  customLoader = null;
  DebuggerServer = null;
});

add_task(function* () {
  gClient = initDebuggerClient();

  const [type] = yield gClient.connect();
  is(type, "browser", "Root actor should identify itself as a browser.");

  const response = yield gClient.getProcess();
  let actor = response.form.actor;
  gThreadClient = yield attachThread(gClient, actor);
  gBrowser.selectedTab = gBrowser.addTab("about:mozilla");

  // listen for a new source and global
  gThreadClient.addListener("newSource", onNewSource);
  gClient.addListener("newGlobal", onNewGlobal);
  yield promise.all([ gNewGlobal.promise, gNewChromeSource.promise ]);

  yield resumeAndCloseConnection();
});
