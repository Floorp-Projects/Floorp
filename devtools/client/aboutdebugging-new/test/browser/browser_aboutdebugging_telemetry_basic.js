/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-telemetry.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-telemetry.js",
  this
);

/**
 * Check that telemetry events are recorded when opening and closing about debugging.
 */
add_task(async function() {
  setupTelemetryTest();

  const { tab } = await openAboutDebugging();

  const openEvents = readAboutDebuggingEvents().filter(
    e => e.method === "open_adbg"
  );
  is(
    openEvents.length,
    1,
    "Exactly one open event was logged for about:debugging"
  );
  const sessionId = openEvents[0].extras.session_id;
  ok(!isNaN(sessionId), "Open event has a valid session id");

  await removeTab(tab);

  const closeEvents = readAboutDebuggingEvents().filter(
    e => e.method === "close_adbg"
  );
  is(
    closeEvents.length,
    1,
    "Exactly one close event was logged for about:debugging"
  );
  is(
    closeEvents[0].extras.session_id,
    sessionId,
    "Close event has the same session id as the open event"
  );
});
