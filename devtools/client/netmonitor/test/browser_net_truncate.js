/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verifies that truncated response bodies still have the correct reported size.
 */

function test() {
  let { L10N } = require("devtools/client/netmonitor/l10n");
  const { RESPONSE_BODY_LIMIT } = require("devtools/shared/webconsole/network-monitor");

  const URL = EXAMPLE_URL + "sjs_truncate-test-server.sjs?limit=" + RESPONSE_BODY_LIMIT;

  // Another slow test on Linux debug.
  requestLongerTimeout(2);

  initNetMonitor(URL).then(({ tab, monitor }) => {
    info("Starting test... ");

    let { NetMonitorView } = monitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(monitor, 1)
      .then(() => teardown(monitor))
      .then(finish);

    monitor.panelWin.once(monitor.panelWin.EVENTS.RECEIVED_RESPONSE_CONTENT, () => {
      let requestItem = RequestsMenu.getItemAtIndex(0);

      verifyRequestItemTarget(RequestsMenu, requestItem, "GET", URL, {
        type: "plain",
        fullMimeType: "text/plain; charset=utf-8",
        transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeMB", 2),
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeMB", 2),
      });
    });

    tab.linkedBrowser.reload();
  });
}
