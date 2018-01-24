/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if visible columns are properly saved
 */

add_task(function* () {
  Services.prefs.setCharPref("devtools.netmonitor.visibleColumns",
    '["status", "contentSize", "waterfall"]');

  let { monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document } = monitor.panelWin;

  ok(document.querySelector("#requests-list-status-button"),
     "Status column should be shown");
  ok(document.querySelector("#requests-list-contentSize-button"),
     "Content size column should be shown");

  yield hideColumn(monitor, "status");
  yield hideColumn(monitor, "contentSize");

  let visibleColumns = JSON.parse(
    Services.prefs.getCharPref("devtools.netmonitor.visibleColumns")
  );

  ok(!visibleColumns.includes("status"),
     "Pref should be synced for status");
  ok(!visibleColumns.includes("contentSize"),
    "Pref should be synced for contentSize");

  yield showColumn(monitor, "status");

  visibleColumns = JSON.parse(
    Services.prefs.getCharPref("devtools.netmonitor.visibleColumns")
  );

  ok(visibleColumns.includes("status"),
    "Pref should be synced for status");
});
