/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the console record the execute_js telemetry event with expected data
// when evaluating expressions.

"use strict";

const TEST_URI = `data:text/html,<!DOCTYPE html><meta charset=utf8>Test execute_js telemetry event`;
const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;

add_task(async function () {
  // Let's reset the counts.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  ok(!snapshot.parent, "No events have been logged for the main process");

  const hud = await openNewTabAndConsole(TEST_URI);

  info("Evaluate a single line");
  await keyboardExecuteAndWaitForResultMessage(hud, `"single line"`, "");

  info("Evaluate another single line");
  await keyboardExecuteAndWaitForResultMessage(hud, `"single line 2"`, "");

  info("Evaluate multiple lines");
  await keyboardExecuteAndWaitForResultMessage(hud, `"n"\n.trim()`, "");

  info("Switch to editor mode");
  await toggleLayout(hud);

  info("Evaluate a single line in editor mode");
  await keyboardExecuteAndWaitForResultMessage(hud, `"single line 3"`, "");

  info("Evaluate multiple lines in editor mode");
  await keyboardExecuteAndWaitForResultMessage(
    hud,
    `"y"\n.trim()\n.trim()`,
    ""
  );

  info("Evaluate multiple lines again in editor mode");
  await keyboardExecuteAndWaitForResultMessage(hud, `"x"\n.trim()`, "");

  checkEventTelemetry([
    getTelemetryEventData({ lines: 1, input: "inline" }),
    getTelemetryEventData({ lines: 1, input: "inline" }),
    getTelemetryEventData({ lines: 2, input: "inline" }),
    getTelemetryEventData({ lines: 1, input: "multiline" }),
    getTelemetryEventData({ lines: 3, input: "multiline" }),
    getTelemetryEventData({ lines: 2, input: "multiline" }),
  ]);

  info("Switch back to inline mode");
  await toggleLayout(hud);
});

function checkEventTelemetry(expectedData) {
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  const events = snapshot.parent.filter(
    event =>
      event[1] === "devtools.main" &&
      event[2] === "execute_js" &&
      event[3] === "webconsole" &&
      event[4] === null
  );

  for (const [i, expected] of expectedData.entries()) {
    const [timestamp, category, method, object, value, extra] = events[i];

    // ignore timestamp
    Assert.greater(timestamp, 0, "timestamp is greater than 0");
    is(category, expected.category, "'category' is correct");
    is(method, expected.method, "'method' is correct");
    is(object, expected.object, "'object' is correct");
    is(value, expected.value, "'value' is correct");
    is(parseInt(extra.lines, 10), expected.extra.lines, "'lines' is correct");
    is(extra.input, expected.extra.input, "'input' is correct");
  }
}

function getTelemetryEventData(extra) {
  return {
    timestamp: null,
    category: "devtools.main",
    method: "execute_js",
    object: "webconsole",
    value: null,
    extra,
  };
}
