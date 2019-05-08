/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests reset column menu item. Note that the column
 * header is visible only if there are requests in the list.
 */
add_task(async function() {
  const { monitor, tab } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const { Prefs } = windowRequire("devtools/client/netmonitor/src/utils/prefs");

  const prefBefore = Prefs.visibleColumns;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  const wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  await wait;

  await hideColumn(monitor, "status");
  await hideColumn(monitor, "waterfall");

  const onRequestsFinished = waitForRequestsFinished(monitor);
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelector("#requests-list-contentSize-button"));

  getContextMenuItem(monitor, "request-list-header-reset-columns").click();
  await onRequestsFinished;

  ok(JSON.stringify(prefBefore) === JSON.stringify(Prefs.visibleColumns),
     "Reset columns item should reset columns pref");
});

/**
 * Helper function for waiting for all events to fire before resolving a promise.
 *
 * @param monitor subject
 *        The event emitter object that is being listened to.
 * @return {Promise}
 *        Returns a promise that resolves upon the last event being fired.
 */
function waitForRequestsFinished(monitor, event) {
  const window = monitor.panelWin;

  return new Promise(resolve => {
    // Key is the request id, value is a boolean - is request finished or not?
    const requests = new Map();

    function onRequest(id) {
      info(`Request ${id} not yet done, keep waiting...`);
      requests.set(id, false);
    }

    function onEventRequest(id) {
      info(`Request ${id} `);
      requests.set(id, true);
      maybeResolve();
    }

    function maybeResolve() {
      // Have all the requests in the map finished yet?
      if ([...requests.values()].some(finished => !finished)) {
        return;
      }

      // All requests are done - unsubscribe from events and resolve!
      window.api.off(EVENTS.NETWORK_EVENT, onRequest);
      window.api.off(EVENTS.RECEIVED_EVENT_TIMINGS, onEventRequest);
      info("All requests finished");
      resolve();
    }

    window.api.on(EVENTS.NETWORK_EVENT, onRequest);
    window.api.on(EVENTS.RECEIVED_EVENT_TIMINGS, onEventRequest);
  });
}
