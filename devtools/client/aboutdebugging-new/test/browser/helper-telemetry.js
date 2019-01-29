/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

/**
 * Reset all telemetry events.
 */
function setupTelemetryTest() {
  // Let's reset the counts.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const OPTOUT = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTOUT;
  const snapshot = Services.telemetry.snapshotEvents(OPTOUT, true);
  ok(!snapshot.parent, "No events have been logged for the main process");
}
/* exported setupTelemetryTest */

/**
 * Retrieve the session id from an "open" event.
 * Note that calling this will "clear" all the events.
 */
function getOpenEventSessionId() {
  const openEvents = readAboutDebuggingEvents().filter(e => e.method === "open_adbg");
  ok(!!openEvents[0], "Found an about:debugging open event");
  return openEvents[0].extras.session_id;
}
/* exported getOpenEventSessionId */

/**
 * Read all the pending events that have "aboutdebugging" as their object property.
 * WARNING: Calling this method also flushes/clears the events.
 */
function readAboutDebuggingEvents() {
  const OPTOUT = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTOUT;
  // Retrieve and clear telemetry events.
  const snapshot = Services.telemetry.snapshotEvents(OPTOUT, true);
  // about:debugging events are logged in the parent process
  const parentEvents = snapshot.parent || [];

  return parentEvents
    .map(_toEventObject)
    .filter(e => e.object === "aboutdebugging");
}
/* exported getLoggedEvents */

/**
 * The telemetry event data structure is simply an array. This helper remaps the array to
 * an object with more user friendly properties.
 */
function _toEventObject(rawEvent) {
  return {
    // Category is typically devtools.main for us.
    category: rawEvent[1],
    // Method is the event's name (eg open, select_page etc...)
    method: rawEvent[2],
    // Object will usually be aboutdebugging for our tests
    object: rawEvent[3],
    // Value is usually empty for devtools events
    value: rawEvent[4],
    // Extras contain all the details of the event, including the session_id.
    extras: rawEvent[5],
  };
}
