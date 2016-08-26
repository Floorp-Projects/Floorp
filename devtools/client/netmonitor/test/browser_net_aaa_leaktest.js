/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the network monitor leaks on initialization and sudden destruction.
 * You can also use this initialization format as a template for other tests.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, NetMonitorView, NetMonitorController } = monitor.panelWin;
  let { RequestsMenu, NetworkDetails } = NetMonitorView;

  ok(tab, "Should have a tab available.");
  ok(monitor, "Should have a network monitor pane available.");

  ok(document, "Should have a document available.");
  ok(NetMonitorView, "Should have a NetMonitorView object available.");
  ok(NetMonitorController, "Should have a NetMonitorController object available.");
  ok(RequestsMenu, "Should have a RequestsMenu object available.");
  ok(NetworkDetails, "Should have a NetworkDetails object available.");

  yield teardown(monitor);
});
