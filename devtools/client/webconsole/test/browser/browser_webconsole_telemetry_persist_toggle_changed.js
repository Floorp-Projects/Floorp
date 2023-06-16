/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the log persistence telemetry event

"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const TEST_URI = `data:text/html,<!DOCTYPE html><meta charset=utf8><script>
  console.log("test message");
</script>`;

add_task(async function () {
  // Let's reset the counts.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  TelemetryTestUtils.assertNumberOfEvents(0);

  const hud = await openNewTabAndConsole(TEST_URI);

  // Toggle persistent logs - "true"
  await toggleConsoleSetting(
    hud,
    ".webconsole-console-settings-menu-item-persistentLogs"
  );
  await waitUntil(
    () => hud.ui.wrapper.getStore().getState().ui.persistLogs === true
  );

  // Toggle persistent logs - "false"
  await toggleConsoleSetting(
    hud,
    ".webconsole-console-settings-menu-item-persistentLogs"
  );
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
