/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if timing intervals are divided againts seconds when appropriate.
 */

function test() {
  initNetMonitor(CUSTOM_GET_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { $all, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 2).then(() => {
      let divisions = $all(".requests-menu-timings-division[division-scale=second]");

      ok(divisions.length,
        "There should be at least one division on the seconds time scale.");
      ok(divisions[0].getAttribute("value").match(/\d+\.\d{2}\s\w+/),
        "The division on the seconds time scale looks legit.");

      teardown(aMonitor).then(finish);
    });

    aDebuggee.performRequests(1);
    window.setTimeout(() => aDebuggee.performRequests(1), 2000);
  });
}
