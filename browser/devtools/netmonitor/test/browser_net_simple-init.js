/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Simple check if the network monitor starts up and shuts down properly.
 */

function test() {
  initNetMonitor(SIMPLE_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    is(aTab.linkedBrowser.contentWindow.wrappedJSObject.location, SIMPLE_URL,
      "The current tab's location is the correct one.");
    is(aDebuggee.location, SIMPLE_URL,
      "The current debuggee's location is the correct one.");

    function checkIfInitialized(aTag) {
      info("Checking if initialization is ok (" + aTag + ").");

      ok(aMonitor._view,
        "The network monitor view object exists (" + aTag + ").");
      ok(aMonitor._view._isInitialized,
        "The network monitor view object exists and is initialized (" + aTag + ").");

      ok(aMonitor._controller,
        "The network monitor controller object exists (" + aTag + ").");
      ok(aMonitor._controller._isInitialized,
        "The network monitor controller object exists and is initialized (" + aTag + ").");

      ok(aMonitor.isReady,
        "The network monitor panel appears to be ready (" + aTag + ").");

      ok(aMonitor._controller.client,
        "There should be a client available at this point (" + aTag + ").");
      ok(aMonitor._controller.tabClient,
        "There should be a tabClient available at this point (" + aTag + ").");
      ok(aMonitor._controller.webConsoleClient,
        "There should be a webConsoleClient available at this point (" + aTag + ").");
    }

    function checkIfDestroyed(aTag) {
      info("Checking if destruction is ok.");

      ok(!aMonitor._controller.client,
        "There shouldn't be a client available after destruction (" + aTag + ").");
      ok(!aMonitor._controller.tabClient,
        "There shouldn't be a tabClient available after destruction (" + aTag + ").");
      ok(!aMonitor._controller.webConsoleClient,
        "There shouldn't be a webConsoleClient available after destruction (" + aTag + ").");
    }

    executeSoon(() => {
      checkIfInitialized(1);

      aMonitor._controller.startupNetMonitor()
        .then(() => {
          info("Starting up again shouldn't do anything special.");
          checkIfInitialized(2);
          return aMonitor._controller.connect();
        })
        .then(() => {
          info("Connecting again shouldn't do anything special.");
          checkIfInitialized(3);
          return teardown(aMonitor);
        })
        .then(finish);
    });

    registerCleanupFunction(() => {
      checkIfDestroyed(1);

      aMonitor._controller.shutdownNetMonitor()
        .then(() => {
          info("Shutting down again shouldn't do anything special.");
          checkIfDestroyed(2);
          return aMonitor._controller.disconnect();
        })
        .then(() => {
          info("Disconnecting again shouldn't do anything special.");
          checkIfDestroyed(3);
        });
    });
  });
}
