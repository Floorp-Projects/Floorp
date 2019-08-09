/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the filters_changed telemetry event.

"use strict";

const TEST_URI = `data:text/html,<meta charset=utf8><script>
  console.log("test message");
</script>`;

const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;

add_task(async function() {
  // Let's reset the counts.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  ok(!snapshot.parent, "No events have been logged for the main process");

  const hud = await openNewTabAndConsole(TEST_URI);

  info("Click on the 'log' filter");
  await setFilterState(hud, {
    log: false,
  });

  checkTelemetryEvent({
    trigger: "log",
    active: "error,warn,info,debug",
    inactive: "text,log,css,net,netxhr",
  });

  info("Click on the 'netxhr' filter");
  await setFilterState(hud, {
    netxhr: true,
  });

  checkTelemetryEvent({
    trigger: "netxhr",
    active: "error,warn,info,debug,netxhr",
    inactive: "text,log,css,net",
  });

  info("Filter the output using the text filter input");
  await setFilterState(hud, { text: "no match" });

  checkTelemetryEvent({
    trigger: "text",
    active: "text,error,warn,info,debug,netxhr",
    inactive: "log,css,net",
  });
});

function checkTelemetryEvent(expectedEvent) {
  const events = getFiltersChangedEventsExtra();
  is(events.length, 1, "There was only 1 event logged");
  const [event] = events;
  ok(event.session_id > 0, "There is a valid session_id in the logged event");
  const f = e => JSON.stringify(e, null, 2);
  is(
    f(event),
    f({
      ...expectedEvent,
      session_id: event.session_id,
    }),
    "The event has the expected data"
  );
}

function getFiltersChangedEventsExtra() {
  // Retrieve and clear telemetry events.
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);

  const filtersChangedEvents = snapshot.parent.filter(
    event =>
      event[1] === "devtools.main" &&
      event[2] === "filters_changed" &&
      event[3] === "webconsole"
  );

  // Since we already know we have the correct event, we only return the `extra` field
  // that was passed to it (which is event[5] here).
  return filtersChangedEvents.map(event => event[5]);
}
