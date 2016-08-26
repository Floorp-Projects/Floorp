/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the empty-requests reload button works.
 */

add_task(function* () {
  let [,, monitor] = yield initNetMonitor(SINGLE_GET_URL);
  info("Starting test... ");

  let { document, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  let wait = waitForNetworkEvents(monitor, 2);
  let button = document.querySelector("#requests-menu-reload-notice-button");
  button.click();
  yield wait;

  is(RequestsMenu.itemCount, 2, "The request menu should have two items after reloading");

  return teardown(monitor);
});
