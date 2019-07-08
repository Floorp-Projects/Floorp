/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the DEVTOOLS_JAVASCRIPT_ERROR_DISPLAYED telemetry event.

"use strict";

const TEST_URI = `data:text/html,<meta charset=utf8><script>document()</script>`;

add_task(async function() {
  startTelemetry();

  const hud = await openNewTabAndConsole(TEST_URI);

  info(
    "Check that the error message is logged in telemetry with the expected key"
  );
  await waitFor(() => findMessage(hud, "is not a function"));
  checkErrorDisplayedTelemetry("JSMSG_NOT_FUNCTION", 1);

  await refreshTab();

  info("Reloading the page (and having the same error) increments the sum");
  await waitFor(() => findMessage(hud, "is not a function"));
  checkErrorDisplayedTelemetry("JSMSG_NOT_FUNCTION", 2);

  info(
    "Evaluating an expression resulting in the same error increments the sum"
  );
  let onMessage = waitForMessage(hud, "window is not a function");
  hud.jsterm.execute("window()");
  await onMessage;
  checkErrorDisplayedTelemetry("JSMSG_NOT_FUNCTION", 3);

  info(
    "Evaluating an expression resulting in another error is logged in telemetry"
  );
  onMessage = waitForMessage(hud, "repeat count must be non-negative");
  hud.jsterm.execute(`"a".repeat(-1)`);
  await onMessage;
  checkErrorDisplayedTelemetry("JSMSG_NEGATIVE_REPETITION_COUNT", 1);

  onMessage = waitForMessage(hud, "repeat count must be non-negative");
  hud.jsterm.execute(`"b".repeat(-1)`);
  await onMessage;
  checkErrorDisplayedTelemetry("JSMSG_NEGATIVE_REPETITION_COUNT", 2);
});

function checkErrorDisplayedTelemetry(key, count) {
  checkTelemetry(
    "DEVTOOLS_JAVASCRIPT_ERROR_DISPLAYED",
    key,
    { 0: 0, 1: count, 2: 0 },
    "array"
  );
}
