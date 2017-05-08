/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verifies that truncated response bodies still have the correct reported size.
 */

function test() {
  let { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
  const { RESPONSE_BODY_LIMIT } = require("devtools/shared/webconsole/network-monitor");

  const URL = EXAMPLE_URL + "sjs_truncate-test-server.sjs?limit=" + RESPONSE_BODY_LIMIT;

  // Another slow test on Linux debug.
  requestLongerTimeout(2);

  initNetMonitor(URL).then(({ tab, monitor }) => {
    info("Starting test... ");

    let { document, store, windowRequire } = monitor.panelWin;
    let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
    let { EVENTS } = windowRequire("devtools/client/netmonitor/src/constants");
    let {
      getDisplayedRequests,
      getSortedRequests,
    } = windowRequire("devtools/client/netmonitor/src/selectors/index");

    store.dispatch(Actions.batchEnable(false));

    waitForNetworkEvents(monitor, 1)
      .then(() => teardown(monitor))
      .then(finish);

    monitor.panelWin.once(EVENTS.RECEIVED_RESPONSE_CONTENT, () => {
      verifyRequestItemTarget(
        document,
        getDisplayedRequests(store.getState()),
        getSortedRequests(store.getState()).get(0),
        "GET", URL,
        {
          type: "plain",
          fullMimeType: "text/plain; charset=utf-8",
          transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeMB", 2),
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeMB", 2),
        }
      );
    });

    tab.linkedBrowser.reload();
  });
}
