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

  hideColumn("status");
  hideColumn("waterfall");

  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelector("#requests-list-contentSize-button"));

  parent.document.querySelector("#request-list-header-reset-columns").click();

  is(JSON.stringify(prefBefore), JSON.stringify(Prefs.visibleColumns),
     "Reset columns item should reset columns pref");

  function* hideColumn(column) {
    info(`Clicking context-menu item for ${column}`);
    EventUtils.sendMouseEvent({ type: "contextmenu" },
      document.querySelector("#requests-list-contentSize-button"));

    let onHeaderRemoved = waitForDOM(document, `#requests-list-${column}-button`, 0);
    parent.document.querySelector(`#request-list-header-${column}-toggle`).click();

    yield onHeaderRemoved;
    ok(!document.querySelector(`#requests-list-${column}-button`),
       `Column ${column} should be hidden`);
  }
});
