/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if toggling the details pane works as expected.
 */

function test() {
  initNetMonitor(SIMPLE_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { document, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    is(document.querySelector("#details-pane-toggle")
      .hasAttribute("disabled"), true,
      "The pane toggle button should be disabled when the frontend is opened.");
    is(document.querySelector("#details-pane-toggle")
      .hasAttribute("pane-collapsed"), true,
      "The pane toggle button should indicate that the details pane is " +
      "collapsed when the frontend is opened.");
    is(NetMonitorView.detailsPaneHidden, true,
      "The details pane should be hidden when the frontend is opened.");
    is(RequestsMenu.selectedItem, null,
      "There should be no selected item in the requests menu.");

    aMonitor.panelWin.once(aMonitor.panelWin.EVENTS.NETWORK_EVENT, () => {
      is(document.querySelector("#details-pane-toggle")
        .hasAttribute("disabled"), false,
        "The pane toggle button should be enabled after the first request.");
      is(document.querySelector("#details-pane-toggle")
        .hasAttribute("pane-collapsed"), true,
        "The pane toggle button should still indicate that the details pane is " +
        "collapsed after the first request.");
      is(NetMonitorView.detailsPaneHidden, true,
        "The details pane should still be hidden after the first request.");
      is(RequestsMenu.selectedItem, null,
        "There should still be no selected item in the requests menu.");

      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.getElementById("details-pane-toggle"));

      is(document.querySelector("#details-pane-toggle")
        .hasAttribute("disabled"), false,
        "The pane toggle button should still be enabled after being pressed.");
      is(document.querySelector("#details-pane-toggle")
        .hasAttribute("pane-collapsed"), false,
        "The pane toggle button should now indicate that the details pane is " +
        "not collapsed anymore after being pressed.");
      is(NetMonitorView.detailsPaneHidden, false,
        "The details pane should not be hidden after toggle button was pressed.");
      isnot(RequestsMenu.selectedItem, null,
        "There should be a selected item in the requests menu.");
      is(RequestsMenu.selectedIndex, 0,
        "The first item should be selected in the requests menu.");

      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.getElementById("details-pane-toggle"));

      is(document.querySelector("#details-pane-toggle")
        .hasAttribute("disabled"), false,
        "The pane toggle button should still be enabled after being pressed again.");
      is(document.querySelector("#details-pane-toggle")
        .hasAttribute("pane-collapsed"), true,
        "The pane toggle button should now indicate that the details pane is " +
        "collapsed after being pressed again.");
      is(NetMonitorView.detailsPaneHidden, true,
        "The details pane should now be hidden after the toggle button was pressed again.");
      is(RequestsMenu.selectedItem, null,
        "There should now be no selected item in the requests menu.");

      teardown(aMonitor).then(finish);
    });

    aDebuggee.location.reload();
  });
}
