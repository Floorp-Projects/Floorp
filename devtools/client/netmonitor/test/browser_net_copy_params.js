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

    Task.spawn(function* () {
      yield waitForNetworkEvents(aMonitor, 1, 6);

      RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(0);
      yield testCopyUrlParamsHidden(false);
      yield testCopyUrlParams("a");
      yield testCopyPostDataHidden(false);
      yield testCopyPostData("{ \"foo\": \"bar\" }");

      RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(1);
      yield testCopyUrlParamsHidden(false);
      yield testCopyUrlParams("a=b");
      yield testCopyPostDataHidden(false);
      yield testCopyPostData("{ \"foo\": \"bar\" }");

      RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(2);
      yield testCopyUrlParamsHidden(false);
      yield testCopyUrlParams("a=b");
      yield testCopyPostDataHidden(false);
      yield testCopyPostData("foo=bar");

      RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(3);
      yield testCopyUrlParamsHidden(false);
      yield testCopyUrlParams("a");
      yield testCopyPostDataHidden(false);
      yield testCopyPostData("{ \"foo\": \"bar\" }");

      RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(4);
      yield testCopyUrlParamsHidden(false);
      yield testCopyUrlParams("a=b");
      yield testCopyPostDataHidden(false);
      yield testCopyPostData("{ \"foo\": \"bar\" }");

      RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(5);
      yield testCopyUrlParamsHidden(false);
      yield testCopyUrlParams("a=b");
      yield testCopyPostDataHidden(false);
      yield testCopyPostData("?foo=bar");

      RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(6);
      yield testCopyUrlParamsHidden(true);
      yield testCopyPostDataHidden(true);

      yield teardown(aMonitor);
      finish();
    });

    function testCopyUrlParamsHidden(aHidden) {
      RequestsMenu._onContextShowing();
      is(document.querySelector("#request-menu-context-copy-url-params").hidden,
        aHidden, "The \"Copy URL Parameters\" context menu item should" + (aHidden ? " " : " not ") + "be hidden.");
    }

    function testCopyUrlParams(aQueryString) {
      RequestsMenu.copyUrlParams();
      is(SpecialPowers.getClipboardData("text/unicode"),
        aQueryString, "The url query string copied from the selected item is correct.");
    }

    function testCopyPostDataHidden(aHidden) {
      RequestsMenu._onContextShowing();
      is(document.querySelector("#request-menu-context-copy-post-data").hidden,
        aHidden, "The \"Copy POST Data\" context menu item should" + (aHidden ? " " : " not ") + "be hidden.");
    }

    function testCopyPostData(aPostData) {
      return RequestsMenu.copyPostData().then(() => {
        is(SpecialPowers.getClipboardData("text/unicode"),
          aPostData, "The post data string copied from the selected item is correct.");
      });
    }

    aDebuggee.performRequests();
  });
}

