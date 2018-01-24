/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests reset column menu item
 */
add_task(function* () {
  let { monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, parent, windowRequire } = monitor.panelWin;
  let { Prefs } = windowRequire("devtools/client/netmonitor/src/utils/prefs");

  let prefBefore = Prefs.visibleColumns;

  hideColumn(monitor, "status");
  hideColumn(monitor, "waterfall");

  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelector("#requests-list-contentSize-button"));

  parent.document.querySelector("#request-list-header-reset-columns").click();

  ok(JSON.stringify(prefBefore) === JSON.stringify(Prefs.visibleColumns),
     "Reset columns item should reset columns pref");
});
