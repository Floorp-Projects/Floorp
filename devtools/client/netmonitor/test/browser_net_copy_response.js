/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if copying a request's response works.
 */

function test() {
  initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    const EXPECTED_RESULT = '{ "greeting": "Hello JSON!" }';

    let { NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 7).then(() => {
      let requestItem = RequestsMenu.getItemAtIndex(3);
      RequestsMenu.selectedItem = requestItem;

      waitForClipboard(EXPECTED_RESULT, function setup() {
        RequestsMenu.copyResponse();
      }, function onSuccess() {
        ok(true, "Clipboard contains the currently selected item's response.");
        cleanUp();
      }, function onFailure() {
        ok(false, "Copying the currently selected item's response was unsuccessful.");
        cleanUp();
      });
    });

    aDebuggee.performRequests();

    function cleanUp(){
      teardown(aMonitor).then(finish);
    }
  });
}
