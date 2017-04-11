/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if hidden columns are properly saved
 */

add_task(function* () {
  Services.prefs.setCharPref("devtools.netmonitor.hiddenColumns",
    '["status", "contentSize"]');

  let { monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, parent } = monitor.panelWin;

  ok(!document.querySelector("#requests-list-status-button"),
     "Status column should be hidden");
  ok(!document.querySelector("#requests-list-contentSize-button"),
     "Content size column should be hidden");

  yield showColumn("status");
  yield showColumn("contentSize");

  ok(!Services.prefs.getCharPref("devtools.netmonitor.hiddenColumns").includes("status"),
    "Pref should be synced for status");
  ok(!Services.prefs.getCharPref("devtools.netmonitor.hiddenColumns")
    .includes("contentSize"), "Pref should be synced for contentSize");

  yield hideColumn("status");

  ok(Services.prefs.getCharPref("devtools.netmonitor.hiddenColumns").includes("status"),
  "Pref should be synced for status");

  yield showColumn("status");

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
