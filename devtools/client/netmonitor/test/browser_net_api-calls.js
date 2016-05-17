/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests whether API call URLs (without a filename) are correctly displayed (including Unicode)
 */

function test() {
  initNetMonitor(API_CALLS_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { document, L10N, EVENTS, Editor, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu, NetworkDetails } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    const REQUEST_URIS = [
      "http://example.com/api/fileName.xml",
      "http://example.com/api/file%E2%98%A2.xml",
      "http://example.com/api/ascii/get/",
      "http://example.com/api/unicode/%E2%98%A2/",
      "http://example.com/api/search/?q=search%E2%98%A2"
    ];

    Task.spawn(function* () {
      yield waitForNetworkEvents(aMonitor, 5);

      REQUEST_URIS.forEach(function (uri, index) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(index), "GET", uri);
      });

      yield teardown(aMonitor);
      finish();
    });

    aDebuggee.performRequests();
  });
}


