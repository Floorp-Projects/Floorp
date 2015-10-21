/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that chrome debugging works.
 */

const TAB_URL = EXAMPLE_URL + "doc_inline-debugger-statement.html";

var gClient, gThreadClient;
var gAttached = promise.defer();
var gNewGlobal = promise.defer()
var gNewChromeSource = promise.defer()

var { DevToolsLoader } = Cu.import("resource://devtools/shared/Loader.jsm", {});
var loader = new DevToolsLoader();
loader.invisibleToDebugger = true;
loader.main("devtools/server/main");
var DebuggerServer = loader.DebuggerServer;

function test() {
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }
  DebuggerServer.allowChromeProcess = true;

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect((aType, aTraits) => {
    is(aType, "browser",
      "Root actor should identify itself as a browser.");

    promise.all([gAttached.promise, gNewGlobal.promise, gNewChromeSource.promise])
      .then(resumeAndCloseConnection)
      .then(finish)
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    testChromeActor();
  });
}

function testChromeActor() {
  gClient.getProcess().then(aResponse => {
    gClient.addListener("newGlobal", onNewGlobal);

    let actor = aResponse.form.actor;
    gClient.attachTab(actor, (response, tabClient) => {
      tabClient.attachThread(null, (aResponse, aThreadClient) => {
        gThreadClient = aThreadClient;
        gThreadClient.addListener("newSource", onNewSource);

        if (aResponse.error) {
          ok(false, "Couldn't attach to the chrome debugger.");
          gAttached.reject();
        } else {
          ok(true, "Attached to the chrome debugger.");
          gAttached.resolve();

          // Ensure that a new chrome global will be created.
          gBrowser.selectedTab = gBrowser.addTab("about:mozilla");
        }
      });
    });
  });
}

function onNewGlobal() {
  ok(true, "Received a new chrome global.");

  gClient.removeListener("newGlobal", onNewGlobal);
  gNewGlobal.resolve();
}

function onNewSource(aEvent, aPacket) {
  if (aPacket.source.url.startsWith("chrome:")) {
    ok(true, "Received a new chrome source: " + aPacket.source.url);

    gThreadClient.removeListener("newSource", onNewSource);
    gNewChromeSource.resolve();
  }
}

function resumeAndCloseConnection() {
  let deferred = promise.defer();
  gThreadClient.resume(() => gClient.close(deferred.resolve));
  return deferred.promise;
}

registerCleanupFunction(function() {
  gClient = null;
  gThreadClient = null;
  gAttached = null;
  gNewGlobal = null;
  gNewChromeSource = null;

  loader = null;
  DebuggerServer = null;
});
