/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const OPTOUT = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTOUT;

/**
 * Test the filters_changed telemetry event.
 */
add_task(async function() {
  const { tab, monitor } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const {
    getDisplayedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  // Remove all telemetry events (you can check about:telemetry).
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const snapshot = Services.telemetry.snapshotEvents(OPTOUT, true);
  ok(!snapshot.parent, "No events have been logged for the main process");

  // Reload to have one request in the list.
  const wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.loadURI(SIMPLE_URL);
  await wait;

  info("Click on the 'HTML' filter");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-html-button"));

  checkTelemetryEvent({
    trigger: "html",
    active: "html",
    inactive: "all,css,js,xhr,fonts,images,media,ws,other",
  });

  info("Click on the 'CSS' filter");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-css-button"));

  checkTelemetryEvent({
    trigger: "css",
    active: "html,css",
    inactive: "all,js,xhr,fonts,images,media,ws,other",
  });

  info("Filter the output using the text filter input");
  setFreetextFilter(monitor, "nomatch");

  // Wait till the text filter is applied.
  await waitUntil(() => getDisplayedRequests(store.getState()).length == 0);

  checkTelemetryEvent({
    trigger: "text",
    active: "html,css",
    inactive: "all,js,xhr,fonts,images,media,ws,other",
  });

  return teardown(monitor);
});

function setFreetextFilter(monitor, value) {
  const { document } = monitor.panelWin;

  const filterBox = document.querySelector(".devtools-filterinput");
  filterBox.focus();
  filterBox.value = "";

  for (const ch of value) {
    EventUtils.synthesizeKey(ch, {}, monitor.panelWin);
  }
}

function checkTelemetryEvent(expectedEvent) {
  const events = getFiltersChangedEventsExtra();
  is(events.length, 1, "There was only 1 event logged");
  const [event] = events;
  ok(event.session_id > 0, "There is a valid session_id in the logged event");
  const f = e => JSON.stringify(e, null, 2);
  is(f(event), f({
    ...expectedEvent,
    "session_id": event.session_id
  }), "The event has the expected data");
}

function getFiltersChangedEventsExtra() {
  // Retrieve and clear telemetry events.
  const snapshot = Services.telemetry.snapshotEvents(OPTOUT, true);

  const filtersChangedEvents = snapshot.parent.filter(event =>
    event[1] === "devtools.main" &&
    event[2] === "filters_changed" &&
    event[3] === "netmonitor"
  );

  // Since we already know we have the correct event, we only return the `extra` field
  // that was passed to it (which is event[5] here).
  return filtersChangedEvents.map(event => event[5]);
}
