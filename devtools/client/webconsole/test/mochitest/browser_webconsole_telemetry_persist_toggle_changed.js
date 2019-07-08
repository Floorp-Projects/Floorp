/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the log persistence telemetry event

"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const TEST_URI = `data:text/html,<meta charset=utf8><script>
  console.log("test message");
</script>`;

add_task(async function() {
  // Let's reset the counts.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  TelemetryTestUtils.assertNumberOfEvents(0);

  const hud = await openNewTabAndConsole(TEST_URI);

  // Get log persistence toggle button
  const logPersistToggle = await waitFor(() =>
    hud.ui.window.document.querySelector(".filter-checkbox")
  );

  // Click on the toggle - "true"
  logPersistToggle.click();
  await waitUntil(
    () => hud.ui.wrapper.getStore().getState().ui.persistLogs === true
  );

  // Click a second time - "false"
  logPersistToggle.click();
  await waitUntil(
    () => hud.ui.wrapper.getStore().getState().ui.persistLogs === false
  );

  const expectedEvents = [
    {
      category: "devtools.main",
      method: "persist_changed",
      object: "webconsole",
      value: "true",
    },
    {
      category: "devtools.main",
      method: "persist_changed",
      object: "webconsole",
      value: "false",
    },
  ];

  const filter = {
    category: "devtools.main",
    method: "persist_changed",
    object: "webconsole",
  };

  // Will compare filtered events to event list above
  await TelemetryTestUtils.assertEvents(expectedEvents, filter);
});
