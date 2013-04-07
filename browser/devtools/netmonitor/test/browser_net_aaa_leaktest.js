/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the network monitor leaks on initialization and sudden destruction.
 * You can also use this initialization format as a template for other tests.
 */

function test() {
  initNetMonitor(SIMPLE_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { document, NetMonitorView, NetMonitorController } = aMonitor.panelWin;
    let { RequestsMenu, NetworkDetails } = NetMonitorView;

    ok(aTab, "Should have a tab available.");
    ok(aDebuggee, "Should have a debuggee available.");
    ok(aMonitor, "Should have a network monitor pane available.");

    ok(document, "Should have a document available.");
    ok(NetMonitorView, "Should have a NetMonitorView object available.");
    ok(NetMonitorController, "Should have a NetMonitorController object available.");
    ok(RequestsMenu, "Should have a RequestsMenu object available.");
    ok(NetworkDetails, "Should have a NetworkDetails object available.");

    teardown(aMonitor).then(finish);
  });
}
