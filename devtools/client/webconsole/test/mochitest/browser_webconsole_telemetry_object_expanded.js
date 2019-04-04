/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the object_expanded telemetry event.

"use strict";

const TEST_URI = `data:text/html,<meta charset=utf8><script>
  console.log("test message", [1,2,3]);
</script>`;

const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;

add_task(async function() {
  // Let's reset the counts.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  ok(!snapshot.parent, "No events have been logged for the main process");

  const hud = await openNewTabAndConsole(TEST_URI);

  const message = await waitFor(() => findMessage(hud, "test message"));

  info("Click on the arrow icon to expand the node");
  const arrowIcon = message.querySelector(".arrow");
  arrowIcon.click();

  // let's wait until we have 2 arrows (i.e. the object was expanded)
  await waitFor(() => message.querySelectorAll(".arrow").length === 2);

  let events = getObjectExpandedEventsExtra();
  is(events.length, 1, "There was 1 event logged");
  const [event] = events;
  ok(event.session_id > 0, "There is a valid session_id in the logged event");

  info("Click on the second arrow icon to expand the prototype node");
  const secondArrowIcon = message.querySelectorAll(".arrow")[1];
  secondArrowIcon.click();
  // let's wait until we have more than 2 arrows displayed, i.e. the prototype node was
  // expanded.
  await waitFor(() => message.querySelectorAll(".arrow").length > 2);

  events = getObjectExpandedEventsExtra();
  is(events.length, 1, "There was an event logged when expanding a child node");

  info("Click the first arrow to collapse the object");
  arrowIcon.click();
  // Let's wait until there's only one arrow visible, i.e. the node is collapsed.
  await waitFor(() => message.querySelectorAll(".arrow").length === 1);

  ok(!snapshot.parent, "There was no event logged when collapsing the node");
});

function getObjectExpandedEventsExtra() {
  // Retrieve and clear telemetry events.
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);

  const events = snapshot.parent.filter(event =>
    event[1] === "devtools.main" &&
    event[2] === "object_expanded" &&
    event[3] === "webconsole"
  );

  // Since we already know we have the correct event, we only return the `extra` field
  // that was passed to it (which is event[5] here).
  return events.map(event => event[5]);
}
