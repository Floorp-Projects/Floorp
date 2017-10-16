/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the pause/resume button works.
 */
add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(PAUSE_URL);
  info("Starting test... ");

  let { document, store, windowRequire, connector } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let pauseButton = document.querySelector(".requests-list-pause-button");

  store.dispatch(Actions.batchEnable(false));

  // Make sure we start in a sane state.
  assertRequestCount(store, 0);

  // Load one request and assert it shows up in the list.
  yield performRequestAndWait(tab, monitor);
  assertRequestCount(store, 1);

  let noRequest = true;
  monitor.panelWin.once(EVENTS.NETWORK_EVENT, () => {
    noRequest = false;
  });

  monitor.panelWin.once(EVENTS.NETWORK_EVENT_UPDATED, () => {
    noRequest = false;
  });

  // Click pause, load second request and make sure they don't show up.
  EventUtils.sendMouseEvent({ type: "click" }, pauseButton);
  yield performPausedRequest(connector, tab, monitor);
  ok(noRequest, "There should be no activity when paused.");
  assertRequestCount(store, 1);

  // Click pause again to resume monitoring. Load a third request
  // and make sure they will show up.
  EventUtils.sendMouseEvent({ type: "click" }, pauseButton);
  yield performRequestAndWait(tab, monitor);
  assertRequestCount(store, 2);

  return teardown(monitor);
});

/**
 * Asserts the number of requests in the network monitor.
 */
function assertRequestCount(store, count) {
  is(store.getState().requests.requests.size, count,
    "There should be correct number of requests");
}

/**
 * Execute simple GET request and wait till it's done.
 */
function* performRequestAndWait(tab, monitor) {
  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, SIMPLE_SJS, function* (url) {
    yield content.wrappedJSObject.performRequests(url);
  });
  yield wait;
}

/**
 * Execute simple GET request
 */
function* performPausedRequest(connector, tab, monitor) {
  let wait = waitForWebConsoleNetworkEvent(connector);
  yield ContentTask.spawn(tab.linkedBrowser, SIMPLE_SJS, function* (url) {
    yield content.wrappedJSObject.performRequests(url);
  });
  yield wait;
}

/**
 * Listen for events fired by the console client since the Firefox
 * connector (data provider) is paused.
 */
function waitForWebConsoleNetworkEvent(connector) {
  return new Promise(resolve => {
    connector.connector.webConsoleClient.once("networkEvent", (type, networkInfo) => {
      resolve();
    });
  });
}
