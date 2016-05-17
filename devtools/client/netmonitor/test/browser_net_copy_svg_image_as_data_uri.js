/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if copying an image as data uri works.
 */

function test() {
  const SVG_URL = EXAMPLE_URL + "dropmarker.svg";
  initNetMonitor(CURL_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 1).then(() => {
      let requestItem = RequestsMenu.getItemAtIndex(0);
      RequestsMenu.selectedItem = requestItem;

      waitForClipboard(function check(text) {
        return text.startsWith("data:") && !/undefined/.test(text);
      }, function setup() {
        RequestsMenu.copyImageAsDataUri();
      }, function onSuccess() {
        ok(true, "Clipboard contains a valid data: URI");
        cleanUp();
      }, function onFailure() {
        ok(false, "Clipboard contains an invalid data: URI");
        cleanUp();
      });
    });

    aDebuggee.performRequest(SVG_URL);

    function cleanUp() {
      teardown(aMonitor).then(finish);
    }
  });
}
