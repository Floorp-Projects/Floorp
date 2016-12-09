/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Simple check if the network monitor starts up and shuts down properly.
 */

function test() {
  // These test suite functions are removed from the global scope inside a
  // cleanup function. However, we still need them.
  let gInfo = info;
  let gOk = ok;

  initNetMonitor(SIMPLE_URL).then(({ tab, monitor }) => {
    info("Starting test... ");

    is(tab.linkedBrowser.currentURI.spec, SIMPLE_URL,
      "The current tab's location is the correct one.");

    function checkIfInitialized(tag) {
      info(`Checking if initialization is ok (${tag}).`);

      ok(monitor._view,
        `The network monitor view object exists (${tag}).`);
      ok(monitor._controller,
        `The network monitor controller object exists (${tag}).`);
      ok(monitor._controller._startup,
        `The network monitor controller object exists and is initialized (${tag}).`);

      ok(monitor.isReady,
        `The network monitor panel appears to be ready (${tag}).`);

      ok(monitor._controller.tabClient,
        `There should be a tabClient available at this point (${tag}).`);
      ok(monitor._controller.webConsoleClient,
        `There should be a webConsoleClient available at this point (${tag}).`);
      ok(monitor._controller.timelineFront,
        `There should be a timelineFront available at this point (${tag}).`);
    }

    function checkIfDestroyed(tag) {
      gInfo("Checking if destruction is ok.");

      gOk(monitor._view,
        `The network monitor view object still exists (${tag}).`);
      gOk(monitor._controller,
        `The network monitor controller object still exists (${tag}).`);
      gOk(monitor._controller._shutdown,
        `The network monitor controller object still exists and is destroyed (${tag}).`);

      gOk(!monitor._controller.tabClient,
        `There shouldn't be a tabClient available after destruction (${tag}).`);
      gOk(!monitor._controller.webConsoleClient,
        `There shouldn't be a webConsoleClient available after destruction (${tag}).`);
      gOk(!monitor._controller.timelineFront,
        `There shouldn't be a timelineFront available after destruction (${tag}).`);
    }

    executeSoon(() => {
      checkIfInitialized(1);

      monitor._controller.startupNetMonitor()
        .then(() => {
          info("Starting up again shouldn't do anything special.");
          checkIfInitialized(2);
          return monitor._controller.connect();
        })
        .then(() => {
          info("Connecting again shouldn't do anything special.");
          checkIfInitialized(3);
          return teardown(monitor);
        })
        .then(finish);
    });

    registerCleanupFunction(() => {
      checkIfDestroyed(1);

      monitor._controller.shutdownNetMonitor()
        .then(() => {
          gInfo("Shutting down again shouldn't do anything special.");
          checkIfDestroyed(2);
          return monitor._controller.disconnect();
        })
        .then(() => {
          gInfo("Disconnecting again shouldn't do anything special.");
          checkIfDestroyed(3);
        });
    });
  });
}
