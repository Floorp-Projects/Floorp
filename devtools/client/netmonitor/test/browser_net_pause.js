/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the pause/resume button works.
 */
add_task(async function() {
  const { tab, monitor, toolbox } = await initNetMonitor(PAUSE_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const pauseButton = document.querySelector(".requests-list-pause-button");

  store.dispatch(Actions.batchEnable(false));

  // Make sure we start in a sane state.
  assertRequestCount(store, 0);

  // Load one request and assert it shows up in the list.
  await performRequestAndWait(tab, monitor);
  assertRequestCount(store, 1);

  let noRequest = true;
  monitor.panelWin.api.once(TEST_EVENTS.NETWORK_EVENT, () => {
    noRequest = false;
  });

  monitor.panelWin.api.once(TEST_EVENTS.NETWORK_EVENT_UPDATED, () => {
    noRequest = false;
  });

  // Click pause, load second request and make sure they don't show up.
  EventUtils.sendMouseEvent({ type: "click" }, pauseButton);
  await performPausedRequest(tab, monitor, toolbox);
  ok(noRequest, "There should be no activity when paused.");
  assertRequestCount(store, 1);

  // Click pause again to resume monitoring. Load a third request
  // and make sure they will show up.
  EventUtils.sendMouseEvent({ type: "click" }, pauseButton);
  await performRequestAndWait(tab, monitor);
  assertRequestCount(store, 2);

  // Click pause, reload the page and check that there are
  // some requests. Page reload should auto-resume.
  EventUtils.sendMouseEvent({ type: "click" }, pauseButton);
  const networkEvents = waitForNetworkEvents(monitor, 1);
  await reloadBrowser();
  await networkEvents;
  assertRequestCount(store, 1);

  return teardown(monitor);
});

/**
 * Asserts the number of requests in the network monitor.
 */
function assertRequestCount(store, count) {
  is(
    store.getState().requests.requests.length,
    count,
    "There should be correct number of requests"
  );
}

/**
 * Execute simple GET request and wait till it's done.
 */
async function performRequestAndWait(tab, monitor) {
  const wait = waitForNetworkEvents(monitor, 1);
  await SpecialPowers.spawn(tab.linkedBrowser, [SIMPLE_SJS], async function(
    url
  ) {
    await content.wrappedJSObject.performRequests(url);
  });
  await wait;
}

/**
 * Execute simple GET request
 */
async function performPausedRequest(tab, monitor, toolbox) {
  const {
    onResource: waitForEventWhenPaused,
  } = await toolbox.resourceCommand.waitForNextResource(
    toolbox.resourceCommand.TYPES.NETWORK_EVENT,
    {
      ignoreExistingResources: true,
    }
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [SIMPLE_SJS], async function(
    url
  ) {
    await content.wrappedJSObject.performRequests(url);
  });
  // Wait for NETWORK_EVENT resources to be fetched, in order to ensure
  // that there is no pending request related to their processing.
  await waitForEventWhenPaused;
}
