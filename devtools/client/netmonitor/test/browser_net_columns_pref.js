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

  let { document, parent } = monitor.panelWin;

  ok(document.querySelector("#requests-list-status-button"),
     "Status column should be shown");
  ok(document.querySelector("#requests-list-contentSize-button"),
     "Content size column should be shown");

  yield hideColumn("status");
  yield hideColumn("contentSize");

  let visibleColumns = JSON.parse(
    Services.prefs.getCharPref("devtools.netmonitor.visibleColumns")
  );

  ok(!visibleColumns.includes("status"),
     "Pref should be synced for status");
  ok(!visibleColumns.includes("contentSize"),
    "Pref should be synced for contentSize");

  yield showColumn("status");

  visibleColumns = JSON.parse(
    Services.prefs.getCharPref("devtools.netmonitor.visibleColumns")
  );

  ok(visibleColumns.includes("status"),
    "Pref should be synced for status");

  function* hideColumn(column) {
    info(`Clicking context-menu item for ${column}`);
    EventUtils.sendMouseEvent({ type: "contextmenu" },
      document.querySelector("#requests-list-status-button") ||
      document.querySelector("#requests-list-waterfall-button"));

    let onHeaderRemoved = waitForDOM(document, `#requests-list-${column}-button`, 0);
    parent.document.querySelector(`#request-list-header-${column}-toggle`).click();

    yield onHeaderRemoved;
    ok(!document.querySelector(`#requests-list-${column}-button`),
       `Column ${column} should be hidden`);
  }

  function* showColumn(column) {
    info(`Clicking context-menu item for ${column}`);
    EventUtils.sendMouseEvent({ type: "contextmenu" },
      document.querySelector("#requests-list-status-button") ||
      document.querySelector("#requests-list-waterfall-button"));

    let onHeaderAdded = waitForDOM(document, `#requests-list-${column}-button`, 1);
    parent.document.querySelector(`#request-list-header-${column}-toggle`).click();

    yield onHeaderAdded;
    ok(document.querySelector(`#requests-list-${column}-button`),
       `Column ${column} should be visible`);
  }
});
