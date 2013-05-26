/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if requests are handled correctly.
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
    is(document.querySelector("#requests-menu-empty-notice")
      .hasAttribute("hidden"), false,
      "An empty notice should be displayed when the frontend is opened.");
    is(RequestsMenu.itemCount, 0,
      "The requests menu should be empty when the frontend is opened.");
    is(NetMonitorView.detailsPaneHidden, true,
      "The details pane should be hidden when the frontend is opened.");

    aMonitor.panelWin.once("NetMonitor:NetworkEvent", () => {
      is(document.querySelector("#details-pane-toggle")
        .hasAttribute("disabled"), false,
        "The pane toggle button should be enabled after the first request.");
      is(document.querySelector("#requests-menu-empty-notice")
        .hasAttribute("hidden"), true,
        "The empty notice should be hidden after the first request.");
      is(RequestsMenu.itemCount, 1,
        "The requests menu should not be empty after the first request.");
      is(NetMonitorView.detailsPaneHidden, true,
        "The details pane should still be hidden after the first request.");

      aMonitor.panelWin.once("NetMonitor:NetworkEvent", () => {
        is(document.querySelector("#details-pane-toggle")
          .hasAttribute("disabled"), false,
          "The pane toggle button should be still be enabled after a reload.");
        is(document.querySelector("#requests-menu-empty-notice")
          .hasAttribute("hidden"), true,
          "The empty notice should be still hidden after a reload.");
        is(RequestsMenu.itemCount, 1,
          "The requests menu should not be empty after a reload.");
        is(NetMonitorView.detailsPaneHidden, true,
          "The details pane should still be hidden after a reload.");

        teardown(aMonitor).then(finish);
      });

      aDebuggee.location.reload();
    });

    aDebuggee.location.reload();
  });
}
