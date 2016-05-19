/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if request and response body logging stays on after opening the console.
 */

function test() {
  initNetMonitor(JSON_LONG_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { L10N, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    function verifyRequest(aOffset) {
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(aOffset),
        "GET", CONTENT_TYPE_SJS + "?fmt=json-long", {
          status: 200,
          statusText: "OK",
          type: "json",
          fullMimeType: "text/json; charset=utf-8",
          size: L10N.getFormatStr("networkMenu.sizeKB", L10N.numberWithDecimals(85975 / 1024, 2)),
          time: true
        });
    }

    waitForNetworkEvents(aMonitor, 1).then(() => {
      verifyRequest(0);

      aMonitor._toolbox.once("webconsole-selected", () => {
        aMonitor._toolbox.once("netmonitor-selected", () => {

          waitForNetworkEvents(aMonitor, 1).then(() => {
            waitForNetworkEvents(aMonitor, 1).then(() => {
              verifyRequest(1);
              teardown(aMonitor).then(finish);
            });

            // Perform another batch of requests.
            aDebuggee.performRequests();
          });

          // Reload debugee.
          aDebuggee.location.reload();
        });

        // Switch back to the netmonitor.
        aMonitor._toolbox.selectTool("netmonitor");
      });

      // Switch to the webconsole.
      aMonitor._toolbox.selectTool("webconsole");
    });

    // Perform first batch of requests.
    aDebuggee.performRequests();
  });
}
