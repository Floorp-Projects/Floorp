/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if visible columns are properly saved
 */

add_task(async function() {
  Services.prefs.setCharPref("devtools.netmonitor.visibleColumns",
    '["status", "contentSize", "waterfall"]');

  const { monitor } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  const { document } = monitor.panelWin;

  ok(document.querySelector("#requests-list-status-button"),
     "Status column should be shown");
  ok(document.querySelector("#requests-list-contentSize-button"),
     "Content size column should be shown");

  await hideColumn(monitor, "status");
  await hideColumn(monitor, "contentSize");

  let visibleColumns = JSON.parse(
    Services.prefs.getCharPref("devtools.netmonitor.visibleColumns")
  );

  ok(!visibleColumns.includes("status"),
     "Pref should be synced for status");
  ok(!visibleColumns.includes("contentSize"),
    "Pref should be synced for contentSize");

  await showColumn(monitor, "status");

  visibleColumns = JSON.parse(
    Services.prefs.getCharPref("devtools.netmonitor.visibleColumns")
  );

  ok(visibleColumns.includes("status"),
    "Pref should be synced for status");
});
