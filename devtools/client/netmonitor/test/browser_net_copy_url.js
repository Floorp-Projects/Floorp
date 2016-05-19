/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if copying a request's url works.
 */

function test() {
  initNetMonitor(CUSTOM_GET_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    waitForNetworkEvents(aMonitor, 1).then(() => {
      let requestItem = RequestsMenu.getItemAtIndex(0);
      RequestsMenu.selectedItem = requestItem;

      waitForClipboard(requestItem.attachment.url, function setup() {
        RequestsMenu.copyUrl();
      }, function onSuccess() {
        ok(true, "Clipboard contains the currently selected item's url.");
        cleanUp();
      }, function onFailure() {
        ok(false, "Copying the currently selected item's url was unsuccessful.");
        cleanUp();
      });
    });

    aDebuggee.performRequests(1);

    function cleanUp() {
      teardown(aMonitor).then(finish);
    }
  });
}
