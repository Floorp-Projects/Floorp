/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that chrome debugging works.
 */

const TAB_URL = EXAMPLE_URL + "doc_inline-debugger-statement.html";

let gClient, gThreadClient;
let gAttached = promise.defer();
let gNewGlobal = promise.defer()
let gNewChromeSource = promise.defer()

function test() {
  if (!DebuggerServer.initialized) {
    DebuggerServer.init(() => true);
    DebuggerServer.addBrowserActors();
  }

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
  gClient.listTabs(aResponse => {
    ok(aResponse.chromeDebugger.contains("chromeDebugger"),
      "Chrome debugger actor should identify itself accordingly.");

    gClient.addListener("newGlobal", onNewGlobal);
    gClient.addListener("newSource", onNewSource);

    gClient.attachThread(aResponse.chromeDebugger, (aResponse, aThreadClient) => {
      gThreadClient = aThreadClient;

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
}

function onNewGlobal() {
  ok(true, "Received a new chrome global.");

  gClient.removeListener("newGlobal", onNewGlobal);
  gNewGlobal.resolve();
}

function onNewSource(aEvent, aPacket) {
  if (aPacket.source.url.startsWith("chrome:")) {
    ok(true, "Received a new chrome source: " + aPacket.source.url);

    gClient.removeListener("newSource", onNewSource);
    gNewChromeSource.resolve();
  }
}

function resumeAndCloseConnection() {
  let deferred = promise.defer();
  gThreadClient.resume(() => gClient.close(deferred.resolve));
  return deferred.promise;
}

registerCleanupFunction(function() {
  removeTab(gBrowser.selectedTab);
  gClient = null;
  gThreadClient = null;
  gAttached = null;
  gNewGlobal = null;
  gNewChromeSource = null;
});
