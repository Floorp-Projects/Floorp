/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Simple check if the network monitor starts up and shuts down properly.
 */

function test() {
  // These test suite functions are removed from the global scope inside a
  // cleanup function. However, we still need them.
  let gInfo = info;
  let gOk = ok;

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
      ok(aMonitor._controller,
        "The network monitor controller object exists (" + aTag + ").");
      ok(aMonitor._controller._startup,
        "The network monitor controller object exists and is initialized (" + aTag + ").");

      ok(aMonitor.isReady,
        "The network monitor panel appears to be ready (" + aTag + ").");

      ok(aMonitor._controller.tabClient,
        "There should be a tabClient available at this point (" + aTag + ").");
      ok(aMonitor._controller.webConsoleClient,
        "There should be a webConsoleClient available at this point (" + aTag + ").");
      ok(aMonitor._controller.timelineFront,
        "There should be a timelineFront available at this point (" + aTag + ").");
    }

    function checkIfDestroyed(aTag) {
      gInfo("Checking if destruction is ok.");

      gOk(aMonitor._view,
        "The network monitor view object still exists (" + aTag + ").");
      gOk(aMonitor._controller,
        "The network monitor controller object still exists (" + aTag + ").");
      gOk(aMonitor._controller._shutdown,
        "The network monitor controller object still exists and is destroyed (" + aTag + ").");

      gOk(!aMonitor._controller.tabClient,
        "There shouldn't be a tabClient available after destruction (" + aTag + ").");
      gOk(!aMonitor._controller.webConsoleClient,
        "There shouldn't be a webConsoleClient available after destruction (" + aTag + ").");
      gOk(!aMonitor._controller.timelineFront,
        "There shouldn't be a timelineFront available after destruction (" + aTag + ").");
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
          gInfo("Shutting down again shouldn't do anything special.");
          checkIfDestroyed(2);
          return aMonitor._controller.disconnect();
        })
        .then(() => {
          gInfo("Disconnecting again shouldn't do anything special.");
          checkIfDestroyed(3);
        });
    });
  });
}
