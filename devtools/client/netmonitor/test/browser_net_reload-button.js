/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the empty-requests reload button works.
 */

add_task(function* () {
  let { monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  let wait = waitForNetworkEvents(monitor, 1);
  let button = document.querySelector("#requests-menu-reload-notice-button");
  button.click();
  yield wait;

  is(RequestsMenu.itemCount, 1, "The request menu should have one item after reloading");

  return teardown(monitor);
});
