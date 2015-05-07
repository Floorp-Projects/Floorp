/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests whether copying a request item's parameters works.
 */

function test() {
  initNetMonitor(PARAMS_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { document, L10N, EVENTS, Editor, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu, NetworkDetails } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    Task.spawn(function () {
      yield waitForNetworkEvents(aMonitor, 1, 6);

      RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(0);
      testCopyUrlParamsHidden(false);
      testCopyUrlParams("a");

      RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(1);
      testCopyUrlParamsHidden(false);
      testCopyUrlParams("a=b");

      RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(2);
      testCopyUrlParamsHidden(false);
      testCopyUrlParams("a=b");

      RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(3);
      testCopyUrlParamsHidden(false);
      testCopyUrlParams("a");

      RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(4);
      testCopyUrlParamsHidden(false);
      testCopyUrlParams("a=b");

      RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(5);
      testCopyUrlParamsHidden(false);
      testCopyUrlParams("a=b");

      RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(6);
      testCopyUrlParamsHidden(true);

      yield teardown(aMonitor);
      finish();
    });

    function testCopyUrlParamsHidden(aHidden) {
      RequestsMenu._onContextShowing();
      is(document.querySelector("#request-menu-context-copy-url-params").hidden, aHidden, "The \"Copy URL Params\" context menu item should" + (aHidden ? " " : " not ") + "be hidden.");
    }

    function testCopyUrlParams(aQueryString) {
      RequestsMenu.copyUrlParams();
      is(SpecialPowers.getClipboardData("text/unicode"), aQueryString, "The url query string copied from the selected item is correct.");
    }

    aDebuggee.performRequests();
  });
}

