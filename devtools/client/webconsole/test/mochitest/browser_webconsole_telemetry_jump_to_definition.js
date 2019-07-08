/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the jump_to_definition telemetry event.

"use strict";

const TEST_URI = `data:text/html,<meta charset=utf8><script>
  function x(){}
  console.log("test message", x);
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
  info("Click on the 'jump to definition' button");
  const jumpIcon = message.querySelector(".jump-definition");
  jumpIcon.click();

  const events = getJumpToDefinitionEventsExtra();
  is(events.length, 1, "There was 1 event logged");
  const [event] = events;
  ok(event.session_id > 0, "There is a valid session_id in the logged event");
});

function getJumpToDefinitionEventsExtra() {
  // Retrieve and clear telemetry events.
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);

  const events = snapshot.parent.filter(
    event =>
      event[1] === "devtools.main" &&
      event[2] === "jump_to_definition" &&
      event[3] === "webconsole"
  );

  // Since we already know we have the correct event, we only return the `extra` field
  // that was passed to it (which is event[5] here).
  return events.map(event => event[5]);
}
